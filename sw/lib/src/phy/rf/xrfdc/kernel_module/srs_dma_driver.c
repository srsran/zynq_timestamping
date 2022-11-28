/**
 *
 * \section COPYRIGHT
 *
 * Copyright 2013-2022 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

//#define DEBUG
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_device.h>

#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>

#include <linux/interrupt.h>
#include <linux/irqdomain.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>

#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/ioctl.h>
#include <linux/wait.h>
#include <linux/delay.h>

#define DMA_MAX_BUFFER_LENGTH  32000 // We can transmit up to 8000 IQ samples per transaction (limited by FPGA DAC FIFO block)

static struct class *cl;             // Variable for the device class
static dev_t base_devno;
static atomic_t nof_devs = ATOMIC_INIT(0);

struct dma_buffer_queue;

enum axi_dma_dir {
	AXIS_S2MM,
	AXIS_MM2S
};

/* Structure describing single buffer used in transmit/receive chain */
struct dma_buffer {
	dma_addr_t physaddr;			/* physical (DMA) address of a buffer */
	void*   virtaddr;			/* virtual address of a buffer */
	size_t  alloc_size;			/* Exact size (in bytes) of the allocated memory */
	size_t  tx_size;			/* size in bytes of the data to be transmitted ( <= alloc_size ) */
	struct  dma_buffer_queue *queue;	/* Pointer to a queue containing all dma buffers */
	struct  list_head node;			/* List node (enables moving the buffer between "in_progress"/"completed" lists) */
	struct  work_struct work;		/* Work to be scheduled for submitting this buffer to DMA engine */
	u32     id;				/* A unique id of this buffer */
	struct dma_async_tx_descriptor *desc;	/* dmaengine transaction descriptor */
};

struct dma_buffer_queue {
	spinlock_t list_lock;
	//struct list_head allocated;
	struct list_head pending;
	struct list_head in_progress;
	struct list_head completed;
	unsigned int number_of_buffers;
	struct dma_buffer **buffers;
	wait_queue_head_t waitq;
	u8 initialized;
	atomic_t enabled;
};

struct drv_pdata {
	struct platform_device *pdev;
	const char *mod_name;

	/* device structures */
	dev_t         _devnum;
	struct cdev   _cdev;
	struct device *device;

	/* protects mutable data */
	struct   semaphore sem;
	atomic_t in_use;

	/* dmaengine */
	struct dma_chan *chan;
	enum axi_dma_dir direction;

	/* TX/RX related structures */
	struct dma_buffer_queue  queue;
	struct workqueue_struct *submit_buff_taskq;
};

/* structure holding allocation request received from userspace */
struct buffers_alloc_request {
	u32  num_of_buffers;
	u32  buffer_size;
};

/* Used to exchange buffers between user- and kernel-space.
 *
 * We use the ID of the DMA buffer,
 * because userspace is supposed to first call ioctl(DMA_LOOPBACK_ALLOC_BUFFERS) and mmap(),
 * the latter will return an address associated with a given ID */
struct user_dma_buf_pointer {
	u32  id;
	u32  tx_size;
};

#define SRS_DMA_IOC_MAGIC 'V'

#define SRS_DMA_ALLOC_BUFFERS    _IOW(SRS_DMA_IOC_MAGIC,  0, struct buffers_alloc_request)
#define SRS_DMA_DESTROY_BUFFERS  _IO(SRS_DMA_IOC_MAGIC,   1)
// rx
#define SRS_DMA_GET_RX_BUFFER    _IOR(SRS_DMA_IOC_MAGIC,  2, struct user_dma_buf_pointer)
#define SRS_DMA_PUT_RX_BUFFER    _IOW(SRS_DMA_IOC_MAGIC,  3, struct user_dma_buf_pointer)
// tx
#define SRS_DMA_GET_TX_BUFFER    _IOR(SRS_DMA_IOC_MAGIC,  4, struct user_dma_buf_pointer)
#define SRS_DMA_SEND_TX_BUFFER   _IOWR(SRS_DMA_IOC_MAGIC, 5, struct user_dma_buf_pointer)
// common
#define SRS_DMA_ENABLE_QUEUE     _IO(SRS_DMA_IOC_MAGIC,   6)
#define SRS_DMA_DISABLE_QUEUE    _IO(SRS_DMA_IOC_MAGIC,   7)


/* Sets p to 1, if not set, otherwise nothing.
 * Returns zero if it was not one, non-zero else
 * */
#define test_and_set(p) !atomic_add_unless(p, 1, 1)

#define to_drvdata(p)       container_of(p, struct drv_pdata, _cdev)
#define queue_to_drvdata(q) container_of(q, struct drv_pdata, queue)

void free_trx_dma_buffers(struct drv_pdata *d_info);


// Callback after finishing DMA transfer (atomic context)
static void dma_buffer_complete(void *data)
{
	dma_cookie_t       cookie;
	unsigned long      flags;
	struct dma_buffer *next_buffer;
	struct drv_pdata  *d_info;

	struct dma_buffer       *buffer = (struct dma_buffer *) data;
	struct dma_buffer_queue *queue = buffer->queue;

	pr_debug("completed buf %d\n", buffer->id);

	d_info = queue_to_drvdata(queue);

	if (!atomic_read(&queue->enabled))
	{
		//pr_debug( "warning: queue is already inactive\n");
		return;
	}
	// ensure the cpu will see updated data
	if (d_info->direction == AXIS_S2MM)
	{
		pr_debug("sync memory\n");
		dma_sync_single_for_cpu(&d_info->pdev->dev, buffer->physaddr,
			buffer->tx_size, DMA_FROM_DEVICE);
	}

	// wake up any waiting thread
	spin_lock_irqsave(&queue->list_lock, flags);
	list_del(&buffer->node);
	list_add_tail(&buffer->node, &queue->completed);
	spin_unlock_irqrestore(&queue->list_lock, flags);
	wake_up_interruptible(&queue->waitq);

	spin_lock_irqsave(&queue->list_lock, flags);
	if (list_empty(&queue->pending))
	{
		spin_unlock_irqrestore(&queue->list_lock, flags);
	}
	else
	{
		// start next pending transaction
		next_buffer = list_first_entry(&queue->pending, struct dma_buffer, node);
		list_del(&next_buffer->node);

		cookie = dmaengine_submit(next_buffer->desc);
		if (dma_submit_error(cookie))
		{
			pr_debug( "dma_buffer_complete: dmaengine_submit() failed,"
			" returned code is %d\n", cookie);
			spin_unlock_irqrestore(&queue->list_lock, flags);
		}
		else
		{
			list_add_tail(&next_buffer->node, &queue->in_progress);
			spin_unlock_irqrestore(&queue->list_lock, flags);
			dma_async_issue_pending(d_info->chan);
			pr_debug("submitted buf %d", next_buffer->id);
		}
	}
}

static void srs_dma_util_clear_list(struct list_head *list)
{
	struct dma_buffer *buff, *next_buff;
	list_for_each_entry_safe(buff, next_buff, list, node) {
		list_del_init(&buff->node);
	}
}


static void srs_dma_reset_queue(struct drv_pdata *d_info)
{
	int i;
	spin_lock_irq(&d_info->queue.list_lock);

	srs_dma_util_clear_list(&d_info->queue.pending);
	srs_dma_util_clear_list(&d_info->queue.in_progress);
	srs_dma_util_clear_list(&d_info->queue.completed);

	if (d_info->direction == AXIS_MM2S) {
		for (i = 0; i < d_info->queue.number_of_buffers; i++)
			list_add_tail(&d_info->queue.buffers[i]->node,
			&d_info->queue.completed);
	}
	spin_unlock_irq(&d_info->queue.list_lock);
}

static int srs_dma_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	struct drv_pdata *d_info = to_drvdata(inode->i_cdev);
	if ( down_interruptible(&d_info->sem))
		return -ERESTARTSYS;

	if (test_and_set(&d_info->in_use))
		return -EBUSY;

	filp->private_data = (void*)&d_info->_cdev;
	up(&d_info->sem);
	return ret;
}

static int srs_dma_release(struct inode *inode, struct file *filp)
{
	struct drv_pdata *d_info = to_drvdata(filp->private_data);
	if ( down_interruptible(&d_info->sem))
		return -ERESTARTSYS;

	dmaengine_terminate_all(d_info->chan);
	free_trx_dma_buffers(d_info);
	atomic_set(&d_info->in_use, 0);
	atomic_set(&d_info->queue.enabled, 0);
	up(&d_info->sem);
	return 0;
}

static int srs_dma_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int ret = -EINVAL;
	int i, id;
	struct dma_buffer *buffer = NULL;

	struct drv_pdata *d_info = to_drvdata(filp->private_data);

	if (down_interruptible(&d_info->sem))
		return -ERESTARTSYS;

	// first find the dma buffer memory that user wants to mmap
	id = vma->vm_pgoff;
	for (i = 0; i < d_info->queue.number_of_buffers; i++)
	{
		if (d_info->queue.buffers[i]->id == id)
		{
			buffer = d_info->queue.buffers[i];
			break;
		}
	}
	if (!buffer)
	{
		dev_err(&d_info->pdev->dev, "Requested range is out of driver's allocated memory\n");
		ret = -ENOMEM;
		goto out;
	}
	// map kernel memory into user space vma
	vma->vm_pgoff = 0;
	//vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	ret = dma_mmap_coherent(&d_info->pdev->dev, vma, buffer->virtaddr, buffer->physaddr, buffer->alloc_size);
	if (ret < 0)
	{
		dev_err(&d_info->pdev->dev, "Unable to map buffer memory into userspace, ret = %d\n", ret);
		goto out;
	}
out:
	up(&d_info->sem);
	return ret;
}

int submit_buffer_to_dma(struct drv_pdata *d_info, struct dma_buffer *buffer)
{
	int transfer_size;
	dma_cookie_t cookie;
	enum dma_ctrl_flags flags;
	enum dma_transfer_direction direction;
	struct dma_async_tx_descriptor * desc;

	if (unlikely(!atomic_read(&d_info->queue.enabled)))
		return -EINVAL;

	flags = DMA_CTRL_ACK | DMA_PREP_INTERRUPT;

	if (d_info->direction == AXIS_S2MM)
	{
		direction     = DMA_DEV_TO_MEM;
		transfer_size = buffer->alloc_size;
	}
	else
	{
		direction     = DMA_MEM_TO_DEV;
		transfer_size = buffer->tx_size;
	}
	// prepare transaction
	desc = dmaengine_prep_slave_single(d_info->chan, buffer->physaddr, transfer_size, direction, flags);

	desc->callback = dma_buffer_complete;
	desc->callback_param = buffer;

	spin_lock_irq(&d_info->queue.list_lock);
	if (list_empty(&d_info->queue.in_progress))
	{
		cookie = dmaengine_submit(desc);
		if (dma_submit_error(cookie))
		{
			dev_err(&d_info->pdev->dev, "dmaengine_submit() failed, "
				"returned code is %d\n", cookie);
			spin_unlock_irq(&d_info->queue.list_lock);
			return cookie;
		}
		dev_dbg(&d_info->pdev->dev, "submit_to_dma %d bytes\n", transfer_size);
		list_add_tail(&buffer->node, &d_info->queue.in_progress);
		spin_unlock_irq(&d_info->queue.list_lock);
		dma_async_issue_pending(d_info->chan);
	}
	else
	{
		dev_dbg(&d_info->pdev->dev,  "add_to_pending_list\n");
		buffer->desc = desc;
		list_add_tail(&buffer->node, &d_info->queue.pending);
		spin_unlock_irq(&d_info->queue.list_lock);
	}
	return 0;
}

static void submit_buffer(struct work_struct *w)
{
	struct dma_buffer *buffer;
	struct dma_buffer_queue *queue;
	struct drv_pdata *d_info;

	buffer = container_of(w, struct dma_buffer, work);
	queue = buffer->queue;
	d_info = queue_to_drvdata(queue);
	submit_buffer_to_dma(d_info, buffer);
}

/*Warning: function must be called with held semaphore! */
void free_trx_dma_buffers(struct drv_pdata *d_info)
{
	int i;
	if (!d_info->queue.initialized)
		return;

	if (d_info->queue.buffers)
	{
		spin_lock_irq(&d_info->queue.list_lock);
		srs_dma_util_clear_list(&d_info->queue.pending);
		srs_dma_util_clear_list(&d_info->queue.in_progress);
		srs_dma_util_clear_list(&d_info->queue.completed);
		spin_unlock_irq(&d_info->queue.list_lock);

		for (i = 0; i < d_info->queue.number_of_buffers; i++)
		{
			if (d_info->queue.buffers[i])
			{
				dma_free_coherent(&d_info->pdev->dev,
						d_info->queue.buffers[i]->alloc_size,
						d_info->queue.buffers[i]->virtaddr,
						d_info->queue.buffers[i]->physaddr);
				devm_kfree(&d_info->pdev->dev, d_info->queue.buffers[i]);
			}
		}
		devm_kfree(&d_info->pdev->dev, d_info->queue.buffers);
	}
	d_info->queue.initialized = 0;
}

int allocate_trx_dma_buffers(struct drv_pdata *d_info, struct buffers_alloc_request *alloc_request)
{
	int i = 0;

	if (down_interruptible(&d_info->sem))
		return -ERESTARTSYS;

	d_info->queue.buffers = devm_kzalloc(&d_info->pdev->dev,
				alloc_request->num_of_buffers * sizeof(struct dma_buffer *), GFP_KERNEL);

	if (!d_info->queue.buffers)
	{
		dev_err(&d_info->pdev->dev, "Unable to allocate memory for DMA buffers array\n");
		up(&d_info->sem);
		return -EFAULT;
	}
	d_info->queue.number_of_buffers = alloc_request->num_of_buffers;

	// Allocate memory
	for (i = 0; i < alloc_request->num_of_buffers; i++)
	{
		struct dma_buffer *buffer = devm_kzalloc(&d_info->pdev->dev, sizeof(struct dma_buffer), GFP_KERNEL);
		if (!buffer)
		{
			dev_err(&d_info->pdev->dev, "Unable to allocate memory for dma_buffer struct\n");
			goto ERROR_FREE_MEM;
		}
		buffer->virtaddr = dma_zalloc_coherent(&d_info->pdev->dev, alloc_request->buffer_size,
							&buffer->physaddr, GFP_KERNEL);
		if (IS_ERR(buffer->virtaddr))
		{
			dev_err(&d_info->pdev->dev, "Couldn't allocate memory for DMA buffer, "
				"error %ld\n", PTR_ERR(buffer->virtaddr));
			goto ERROR_FREE_MEM;
		}

		buffer->alloc_size = alloc_request->buffer_size;
		buffer->tx_size    = 0;
		buffer->queue      = &d_info->queue;
		buffer->id         = i;

		d_info->queue.buffers[i] = buffer;
		INIT_LIST_HEAD(&buffer->node);
		INIT_WORK(&buffer->work, submit_buffer);
		//list_add_tail(&buffer->node, &d_info->queue.allocated);
		if (d_info->direction == AXIS_MM2S)
			list_add_tail(&buffer->node, &d_info->queue.completed);
	}

	d_info->queue.initialized = 1;

	up(&d_info->sem);
	return 0;

ERROR_FREE_MEM:
	free_trx_dma_buffers(d_info);
	up(&d_info->sem);
	return -EFAULT;
}

static long srs_dma_cdev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int retval = 0, i = 0;
	struct user_dma_buf_pointer  user_buffer_p;
	struct buffers_alloc_request alloc_request;
	struct dma_buffer *buffer;

	struct drv_pdata *d_info = to_drvdata(filp->private_data);

	if (_IOC_TYPE(cmd) != SRS_DMA_IOC_MAGIC) {
		dev_err(&d_info->pdev->dev, "wrong IOCTL magic number\n");
		return -ENOTTY;
	}

	switch(cmd)
	{
	/* allocate DMA buffers according to allocation request passed from user-spae program */
	case SRS_DMA_ALLOC_BUFFERS:
		if (copy_from_user(&alloc_request, (void __user *)arg, sizeof(alloc_request)) != 0)
		{
			dev_err(&d_info->pdev->dev, "Unable to copy alloc request from userspace\n");
			return -EFAULT;
		}
		retval = allocate_trx_dma_buffers(d_info, &alloc_request);
		return retval;

	/* destroy buffers allocated with SRS_DMA_ALLOC_BUFFERS */
	case SRS_DMA_DESTROY_BUFFERS:
		if (down_interruptible(&d_info->sem))
			return -ERESTARTSYS;

		dmaengine_terminate_all(d_info->chan);
		free_trx_dma_buffers(d_info);
		up(&d_info->sem);
		break;

	/* request one DMA buffer: for RX this means the buffer with received data */
	case SRS_DMA_GET_RX_BUFFER:
		spin_lock_irq(&d_info->queue.list_lock);
		while (atomic_read(&d_info->queue.enabled) && list_empty(&d_info->queue.completed))
		{
			spin_unlock_irq(&d_info->queue.list_lock);
			if (wait_event_interruptible(d_info->queue.waitq, 
						!list_empty(&d_info->queue.completed) || 
						!atomic_read(&d_info->queue.enabled)))
				return -ERESTARTSYS; // interrupted by signal: tell the caller to restart

			spin_lock_irq(&d_info->queue.list_lock);
		}
		/* we could have been woken up by other thread disabling queue, return EFAULT in this case */
		if (!atomic_read(&d_info->queue.enabled))
		{
			spin_unlock_irq(&d_info->queue.list_lock);
			return -EFAULT;
		}

		/* proceed with the new buffer */
		buffer = list_first_entry(&d_info->queue.completed, struct dma_buffer, node);
		list_del(&buffer->node);
		spin_unlock_irq(&d_info->queue.list_lock);

		user_buffer_p.id = buffer->id;
		if (copy_to_user((void __user *)arg, &user_buffer_p, sizeof(user_buffer_p)) != 0)
		{
			dev_err(&d_info->pdev->dev, "Unable to copy user_dma_buffer_pointer to userspace\n");
			return -EFAULT;
		}
		dev_dbg(&d_info->pdev->dev, "got %d\n", user_buffer_p.id);
		break;

	/* request one DMA buffer: for TX this means just a free buffer from the list */
	case SRS_DMA_GET_TX_BUFFER:
		spin_lock_irq(&d_info->queue.list_lock);
		while (list_empty(&d_info->queue.completed))
		{
			spin_unlock_irq(&d_info->queue.list_lock);
			if (wait_event_interruptible(d_info->queue.waitq, !list_empty(&d_info->queue.completed)))
				return -ERESTARTSYS; // interrupted by signal: tell the caller to restart

			spin_lock_irq(&d_info->queue.list_lock);
		}
		buffer = list_first_entry(&d_info->queue.completed, struct dma_buffer, node);
		list_del(&buffer->node);
		spin_unlock_irq(&d_info->queue.list_lock);

		user_buffer_p.id = buffer->id;
		if (copy_to_user((void __user *)arg, &user_buffer_p, sizeof(user_buffer_p)) != 0)
		{
			dev_err(&d_info->pdev->dev, "Unable to copy user_dma_buffer_pointer to userspace\n");
			return -EFAULT;
		}
		dev_dbg(&d_info->pdev->dev, "got %d\n", user_buffer_p.id);
		break;

	/* return buffer to the queue, to be used for data reception */
	case SRS_DMA_PUT_RX_BUFFER:
		// Some sanity checks first
		if (copy_from_user(&user_buffer_p, (void __user *)arg, sizeof(user_buffer_p)) != 0)
		{
			dev_err(&d_info->pdev->dev, "Unable to copy user_dma_buffer_pointer struct from userspace\n");
			return -EFAULT;
		}
		if (unlikely(!d_info->queue.buffers))
		{
			dev_err(&d_info->pdev->dev, "dma buffers are not allocated\n");
			return -EFAULT;
		}
		if (unlikely(user_buffer_p.id < 0 || user_buffer_p.id >= d_info->queue.number_of_buffers))
		{
			dev_err(&d_info->pdev->dev, "Invalid dma buffer ID passed from userspace\n");
			return -EFAULT;
		}
		dev_dbg(&d_info->pdev->dev, "put %d\n", user_buffer_p.id);
		buffer = d_info->queue.buffers[user_buffer_p.id];
		//queue_work(d_info->submit_buff_taskq, &buffer->work);
		submit_buffer_to_dma(d_info, buffer);
		break;

	/* send tx dma buffer, get next free buffer pointer and return it to user-space */
	case SRS_DMA_SEND_TX_BUFFER:
		// Some sanity checks first
		if (copy_from_user(&user_buffer_p, (void __user *)arg, sizeof(user_buffer_p)) != 0)
		{
			dev_err(&d_info->pdev->dev, "Unable to copy user_dma_buffer_pointer struct from userspace\n");
			return -EFAULT;
		}
		if (unlikely(!d_info->queue.buffers))
		{
			dev_err(&d_info->pdev->dev, "dma buffers are not allocated\n");
			return -EFAULT;
		}
		if (unlikely(user_buffer_p.id < 0 || user_buffer_p.id >= d_info->queue.number_of_buffers))
		{
			dev_err(&d_info->pdev->dev, "Invalid dma buffer ID passed from userspace\n");
			return -EFAULT;
		}
		buffer = d_info->queue.buffers[user_buffer_p.id];
		if (unlikely(!buffer))
		{
			dev_err(&d_info->pdev->dev, "dma buffer with id=%d doesn't exist\n", user_buffer_p.id);
			return -EFAULT;
		}
		// 1. submit this buffer to DMA
		buffer->tx_size = user_buffer_p.tx_size;
		retval = submit_buffer_to_dma(d_info, buffer);
		if (retval < 0)
			return retval;
		dev_dbg(&d_info->pdev->dev, "sent %d\n", user_buffer_p.id);

		// 2. return pointer to free buffer back to user-space
		dev_dbg(&d_info->pdev->dev, "get tx\n");

		spin_lock_irq(&d_info->queue.list_lock);
		while (list_empty(&d_info->queue.completed))
		{
			spin_unlock_irq(&d_info->queue.list_lock);
			if (wait_event_interruptible(d_info->queue.waitq, !list_empty(&d_info->queue.completed)))
				return -ERESTARTSYS; // interrupted by signal: tell the caller to restart

			spin_lock_irq(&d_info->queue.list_lock);
		}
		buffer = list_first_entry(&d_info->queue.completed, struct dma_buffer, node);
		list_del(&buffer->node);
		spin_unlock_irq(&d_info->queue.list_lock);

		user_buffer_p.id      = buffer->id;
		user_buffer_p.tx_size = 0;

		if (copy_to_user((void __user *)arg, &user_buffer_p, sizeof(user_buffer_p)) != 0)
		{
			dev_err(&d_info->pdev->dev, "Unable to copy user_dma_buffer_pointer to userspace\n");
			return -EFAULT;
		}
		dev_dbg(&d_info->pdev->dev, "got tx %d\n", user_buffer_p.id);
		return retval;

	/* enable buffers queue: in case of RX this submits all buffers to DMA block */
	case SRS_DMA_ENABLE_QUEUE:
		if (down_interruptible(&d_info->sem))
			return -ERESTARTSYS;

		if (atomic_read(&d_info->queue.enabled))
		{
			up(&d_info->sem);
			break;
		}
		atomic_set(&d_info->queue.enabled, 1);

		if (d_info->direction == AXIS_MM2S)
		{
			up(&d_info->sem);
			break;
		}

		for (i = 0; i < d_info->queue.number_of_buffers; i++)
		{
			buffer = d_info->queue.buffers[i];
			retval = submit_buffer_to_dma(d_info, buffer);
			if (retval)
				goto ERROR_RESET_QUEUE;
		}
		up(&d_info->sem);
		break;

	/* terminate all dma transactions (pending or active) and mark the queue as disabled */
	case SRS_DMA_DISABLE_QUEUE:
		if (down_interruptible(&d_info->sem))
			return -ERESTARTSYS;

		dmaengine_terminate_all(d_info->chan);
		atomic_set(&d_info->queue.enabled, 0);
		srs_dma_reset_queue(d_info);

		wake_up_interruptible(&d_info->queue.waitq);

		up(&d_info->sem);
		dev_dbg(&d_info->pdev->dev, "disable - end\n");
		break;

	/* wrong command */
	default:
		return -ENOTTY;
	}

	return 0;

ERROR_RESET_QUEUE:

	pr_debug("IOCTL ERROR\n");
	dmaengine_terminate_all(d_info->chan);
	atomic_set(&d_info->queue.enabled, 0);

	srs_dma_reset_queue(d_info);

	up(&d_info->sem);
	return retval;
}


static const struct file_operations drv_fops = {
	.open           = srs_dma_open,
	.release        = srs_dma_release,
	.unlocked_ioctl = srs_dma_cdev_ioctl,
	.compat_ioctl   = srs_dma_cdev_ioctl,
	.mmap           = srs_dma_mmap,
	.owner          = THIS_MODULE,
};

/* Allocate and register character device */
static int create_cdev(struct drv_pdata *d_info)
{
	int ret = 0, minor = 0;
	if (!MAJOR(base_devno))
	{
		if (alloc_chrdev_region(&base_devno, 0, 2, "srs_dma_devs") < 0)
		{
			dev_err(&d_info->pdev->dev,"Error in alloc_chrdev_region\n");
			return -1;
		}
	}
	minor = atomic_read(&nof_devs);
	d_info->_devnum = MKDEV(MAJOR(base_devno), minor);

	cdev_init(&d_info->_cdev, &drv_fops);
	d_info->_cdev.owner = THIS_MODULE;

	if ((ret = cdev_add(&d_info->_cdev, d_info->_devnum, 1)))
	{
		dev_err(&d_info->pdev->dev, "Error in in cdev_add\n");
		goto ERR_2;
	}
	atomic_inc(&nof_devs);

	if (!cl)
	{
		cl = class_create(THIS_MODULE, "srs_dma");
		if (IS_ERR(cl))
		{
			dev_err(&d_info->pdev->dev, "Error in class_create\n");
			ret = PTR_ERR(cl);
			goto ERR_2;
		}
	}
	if ((d_info->device = device_create(cl,
					&d_info->pdev->dev,
					d_info->_devnum,
					d_info, d_info->mod_name)) == NULL)
	{
		ret = -ENOMEM;
		goto ERR_1;
	}
	dev_info(&d_info->pdev->dev, "created character device /dev/%s \n", d_info->mod_name);
	return 0;

ERR_1:
	device_destroy(cl, d_info->_devnum);
	class_destroy(cl);
ERR_2:
	cdev_del(&d_info->_cdev);
	d_info->_devnum = MKDEV(0,0);
	unregister_chrdev_region(base_devno, 2);
	return ret;
}

static int create_device(struct drv_pdata *d_info, struct platform_device *pdev)
{
	int ret = 0;
	int num_dma_names, num_dma_phandles, num_dma_directions;

	// 1. Make sure dma references are specified in devicetree entry.
	num_dma_names = of_property_count_strings(pdev->dev.of_node, "dma-names");
	if (num_dma_names == 0)
	{
		dev_err(&pdev->dev,"No DMAs specified in devicetree (\"dma-names\" property is empty)\n");
		return -ENODEV;
	}
	else if (num_dma_names < 0)
	{
		dev_err(&pdev->dev, "got %d when trying to count the elements of \"dma-names\" property\n", num_dma_names);
		return num_dma_names;   // contains error code
	}
	//--
	num_dma_phandles = of_count_phandle_with_args(pdev->dev.of_node, "dmas", "#dma-cells");
	if (num_dma_phandles == 0)
	{
		dev_err(&pdev->dev,"No DMAs specified in devicetree (\"dmas\" property is empty)\n");
		return -ENODEV;
	}
	else if (num_dma_phandles < 0)
	{
		dev_err(&pdev->dev, "got %d when trying to count the elements of \"dmas\" property\n", num_dma_phandles);
		return num_dma_phandles;   // contains error code
	}
	//--
	if (num_dma_names != num_dma_phandles)
	{
		dev_err(&pdev->dev, "Incorrect devicetree, \"dma-names\" and \"dmas\" properties "
				"contain different number of elements\n");
	}
	// read DMA direction specified in the device-tree
	num_dma_directions = of_property_count_strings(pdev->dev.of_node, "dma-direction");
	if (num_dma_directions <= 0)
	{
		dev_err(&pdev->dev,"DMA channel direction is not specified in devicetree "
				"(\"dma-direction\" property is empty)\n");
		return -ENODEV;
	}
	else if (num_dma_directions != num_dma_phandles)
	{
		dev_err(&pdev->dev,"\"dma-direction\" property has different length "
				"then \"dma-names\" and \"dmas\" \n");
		return -ENODEV;
	}

	// 2. Request DMA channel specified in devicetree (if more then one specified,
	//    only the first will be requested, others will be ignored)
	const char *p_dma_name, *p_dma_direction;
	struct dma_chan *chan;

	// Read DMA name property
	ret = of_property_read_string_index(pdev->dev.of_node, "dma-names", 0, &p_dma_name);
	if (ret)
	{
		dev_err(&pdev->dev, "of_property_read_string_index(\"dma-names\", %d) returned %d\n", 0, ret);
		return ret;
	}
	// Get the named DMA channel
	chan = dma_request_slave_channel(&pdev->dev, p_dma_name);
	if (!chan)
	{
		dev_err(&pdev->dev, "Couldn't find DMA channel: %s\n", p_dma_name);
		return -EPROBE_DEFER;
	}
	d_info->chan = chan;

	// Get the direction of this DMA channel
	ret = of_property_read_string_index(pdev->dev.of_node, "dma-direction", 0, &p_dma_direction);
	if (ret)
	{
		dev_err(&pdev->dev, "of_property_read_string_index(\"dma-direction\", %d) returned %d\n", 0, ret);
		return ret;
	}

	if(!strncmp(p_dma_direction, "tx", 2))
	{
		d_info->mod_name  = "srs_tx_dma";
		d_info->direction = AXIS_MM2S;
	}
	else if(!strncmp(p_dma_direction, "rx", 2))
	{
		d_info->mod_name = "srs_rx_dma";
		d_info->direction = AXIS_S2MM;
	}
	else
	{
		dev_err(&pdev->dev, "wrong direction specified in \"dma-direction\" property "
				"(valid options are \"tx\" or \"rx\")\n");
	}
	dev_info(&pdev->dev, "found dma channel: name=\"%s\", direction=\"%s\"\n", p_dma_name, p_dma_direction);

	d_info->pdev = pdev;

	// 3. set DMA coherent mask
	u64 dma_mask;
	dma_mask = DMA_BIT_MASK(64);
	ret = dma_set_coherent_mask(&d_info->pdev->dev, dma_mask);
	if (ret < 0) {
		dev_err(&d_info->pdev->dev, "Unable to set the DMA coherent mask.\n");
		return ret;
	}

	// 4. Initialize other driver's structures
	sema_init(&d_info->sem, 1);
	atomic_set(&d_info->in_use, 0);
	init_waitqueue_head(&d_info->queue.waitq);
	spin_lock_init(&d_info->queue.list_lock);

	d_info->queue.initialized = 0;
	atomic_set(&d_info->queue.enabled, 0);

	//INIT_LIST_HEAD(&d_info->queue.allocated);
	INIT_LIST_HEAD(&d_info->queue.pending);
	INIT_LIST_HEAD(&d_info->queue.in_progress);
	INIT_LIST_HEAD(&d_info->queue.completed);

	/* init workqueue used for scheduling submitting buffers back to DMA engine*/
	d_info->submit_buff_taskq = alloc_workqueue("submit_dma_buffers_wq", WQ_UNBOUND, 1);
	if (!d_info->submit_buff_taskq) {
		ret = -ENOMEM;
		goto ERROR;
	}

	// 5. Create node for this module inside /dev
	if ((ret = create_cdev(d_info)))
		goto ERROR;

	return 0;

ERROR:
	if (d_info->chan)
	{
		dmaengine_terminate_all(d_info->chan);
		dma_release_channel(d_info->chan);
	}
	return ret;
}

static int srs_dma_probe(struct platform_device *pdev)
{
	struct drv_pdata *d_info;
	int ret;
	dev_info(&pdev->dev, "Probing srs-dma driver...\n");

	d_info = devm_kzalloc(&pdev->dev, sizeof(*d_info), GFP_KERNEL);

	if (!d_info)
	{
		dev_err(&pdev->dev, "No memory for device driver data");
		return -ENOMEM;
	}
	if ((ret = create_device(d_info, pdev)))
	{
		if (d_info->device)
		{
			device_destroy(cl, d_info->_devnum);
			cdev_del(&d_info->_cdev);
			d_info->_devnum = MKDEV(0,0);
			class_destroy(cl);
			unregister_chrdev_region(base_devno, 1);
			return ret;
		}
	}

	platform_set_drvdata(pdev, d_info);
	dev_info(&pdev->dev, "Successfully probed!\n");

	return 0;
}

static int srs_dma_remove(struct platform_device *pdev)
{
	struct drv_pdata *d_info = (struct drv_pdata*) platform_get_drvdata(pdev);

	if (d_info->chan)
	{
		dmaengine_terminate_all(d_info->chan);
		dma_release_channel(d_info->chan);
	}

	/* free char device resources */
	if (d_info->device) {
		device_destroy(cl, d_info->_devnum);
		cdev_del(&d_info->_cdev);
		d_info->_devnum = MKDEV(0,0);
		atomic_dec(&nof_devs);
		if (!atomic_read(&nof_devs))
		{
			class_destroy(cl);
			unregister_chrdev_region(base_devno, 2);
		}
	}
	if (d_info->submit_buff_taskq) {
		destroy_workqueue(d_info->submit_buff_taskq);
	}
	dev_info(&pdev->dev, "Device driver removed\n");
	return 0;
}

static const struct of_device_id srs_dma_of_match[] = {
	{ .compatible = "srs,txrx_dma", },
	{}
};

MODULE_DEVICE_TABLE(of, srs_dma_of_match);

static struct platform_driver srs_dma_driver = {
	.driver = {
		.name = "srs_dma_driver",
		.owner = THIS_MODULE,
		.of_match_table = srs_dma_of_match,
	},
	.probe  = srs_dma_probe,
	.remove = srs_dma_remove,
};

module_platform_driver(srs_dma_driver);
MODULE_AUTHOR("SRS");
MODULE_DESCRIPTION("Xilinx AXI DMA proxy driver for interfacing ADC/DACs from the CPU");
MODULE_LICENSE("GPL");
