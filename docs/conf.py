# Configuration file for the Sphinx documentation builder.
import os
import shutil
from sphinx.application import Sphinx

# -- Project information

project = 'LDMS'
copyright = '2025, Sandia National Laboratories and Open Grid Computing, Inc.'
author = 'SNL/OGC'

release = '0.1'
version = '0.1.0'

# -- General configuration

extensions = [
    'sphinx.ext.duration',
    'sphinx.ext.doctest',
    'sphinx.ext.autodoc',
    'sphinx.ext.autosummary',
    'sphinx.ext.intersphinx',
]

from docutils.parsers.rst import roles

def dummy_role(name, rawtext, text, lineno, inliner, options={}, content=[]):
    """A no-op role that prevents errors for unknown roles like :ref: in rst2man."""
    return [], []

# Register the dummy role for 'ref'
roles.register_local_role("ref", dummy_role)

intersphinx_mapping = {
    'python': ('https://docs.python.org/3/', None),
    'sphinx': ('https://www.sphinx-doc.org/en/master/', None),

    # Link to the "apis" of the "hpc-ovis" project and subprojects
    "ovis-hpc": ("https://ovis-hpc.readthedocs.io/en/latest/", None),
    "sos": ("https://ovis-hpc.readthedocs.io/projects/sos/en/latest/", None),
    "maestro": ("https://ovis-hpc.readthedocs.io/projects/maestro/en/latest/", None),
    "baler": ("https://ovis-hpc.readthedocs.io/projects/baler/en/latest/", None),
    "ldms": ("https://ovis-hpc.readthedocs.io/projects/ldms/en/latest/", None),
    "containers": ("https://ovis-hpc.readthedocs.io/projects/containers/en/latest/", None),

}
intersphinx_disabled_domains = ['std']
intersphinx_disabled_reftypes = ["*"]

templates_path = ['_templates']

# -- Options for HTML output

html_theme = 'sphinx_rtd_theme'
html_logo = 'https://ovis-hpc.readthedocs.io/en/latest/_images/ovis-logo.png'
html_theme_options = {
    'logo_only': True,
    'display_version': False,
}

# -- Options for EPUB output
epub_show_urls = 'footnote'

# Define source and destination directories
RST_SRC = os.path.join(os.getcwd(), '..', 'ldms')  # Replace with your actual path
RST_DST = os.path.join(os.getcwd(), 'docs', 'rst_man')

# Function to copy the .rst files
def copy_rst_files(app, env):
    if os.path.exists(RST_DST):
        shutil.rmtree(RST_DST)  # Remove existing destination directory
    os.makedirs(RST_DST)  # Create destination directory
    for root, dirs, files in os.walk(RST_SRC):
        for file in files:
            if file.endswith('.rst'):
                src_file = os.path.join(root, file)
                dst_file = os.path.join(RST_DST, os.path.relpath(src_file, RST_SRC))
                os.makedirs(os.path.dirname(dst_file), exist_ok=True)
                shutil.copy(src_file, dst_file)

# Hook the function into the Sphinx build process
def setup(app: Sphinx):
    app.connect('builder-inited', copy_rst_files)
