# The following has been generated automatically from src/core/vectorfield/qgsvectorfieldarrowsettings.h
# monkey patching scoped based enum
QgsVectorFieldArrowSettings.ArrowScalingMethod.MinMax.__doc__ = ""
QgsVectorFieldArrowSettings.ArrowScalingMethod.Scaled.__doc__ = ""
QgsVectorFieldArrowSettings.ArrowScalingMethod.Fixed.__doc__ = ""
QgsVectorFieldArrowSettings.ArrowScalingMethod.__doc__ = """Use fixed length :py:func:`~QgsVectorFieldArrowSettings.fixedShaftLength` regardless of vector's magnitude

* ``MinMax``: 
* ``Scaled``: 
* ``Fixed``: 

"""
# --
try:
    QgsVectorFieldArrowSettings.__group__ = ['vectorfield']
except (NameError, AttributeError):
    pass
