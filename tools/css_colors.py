"""Compatibility shim. The module is `css_controls` now.

1.0 renamed it along with the manifest block: a package declares controls, and colour
is one kind of control among switches, texture choices, material scalars and springs.
Package builders in CSS-eins0fx-collections still import the old name, and keep working
until their convention pass. Nothing new should import this.
"""
from css_controls import *          # noqa: F401,F403
from css_controls import (          # noqa: F401
    ASSET, BONE, ID, KINDS, RESOURCE, ROLES,
    asset, block, bounded_array, embed, identifier, kind_of, lint_convention,
    number, parameter, resource_info, scalar, spring_defaults, spring_range,
    spring_tuning, validate, vector, verify_resources,
)
