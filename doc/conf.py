# -*- coding: utf-8 -*-

import re
import sys
from json import dump, loads
from os.path import abspath
from pathlib import Path

master_doc = "index"

# If extensions (or modules to document with autodoc) are in another directory,
# add these directories to sys.path here. If the directory is relative to the
# documentation root, use os.path.abspath to make it absolute, like shown here.
sys.path.insert(0, abspath("."))

pygments_style = "sphinx"

# -- Project information -----------------------------------------------------

project = "zynq_timestamping "
copyright = "Software Radio Systems"
author = "Software Radio Systems"

# The full version, including alpha/beta/rc tags
release = "1.0.0"


# -- General configuration ---------------------------------------------------

# Add any Sphinx extension module names here, as strings. They can be
# extensions coming with Sphinx (named 'sphinx.ext.*') or your custom
# ones.
# extensions = [
#     'myst_parser'
# ]
extensions = ["sphinxcontrib.seqdiag", "sphinxcontrib.blockdiag", "sphinx_copybutton"]

# Add any paths that contain templates here, relative to this directory.
templates_path = ["_templates"]

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This pattern also affects html_static_path and html_extra_path.
exclude_patterns = []


# -- Options for HTML output -------------------------------------------------

html_theme_options = {
    "analytics_id": "UA-140380699-2",
    "home_breadcrumbs": False,
    "vcs_pageview_mode": "blob",
    "style_nav_header_background": "#ffffff",
    "logo_only": True,
}

html_context = {}
ctx = Path(__file__).resolve().parent / "context.json"
if ctx.is_file():
    html_context.update(loads(ctx.open("r").read()))

html_theme_path = ["."]
html_theme = "sphinx_rtd_theme"

html_logo_path = ["_images"]

html_logo = str(Path(html_logo_path[0]) / "logo.png")

# html_favicon = str(Path(html_logo_path[0]) / "logo.ico")

# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".

html_static_path = ["_static"]
html_css_files = [
    "custom.css",
]
source_suffix = [".rst"]
# Output file base name for HTML help builder.
htmlhelp_basename = "srs_libiio_timestamps"
