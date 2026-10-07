"""Temporary CW tongue-tip controls for source sampling, never saved to the rig."""
import json


class PerformanceControls:
    def __init__(self, rig):
        self.rig = rig
        self.drivers = []
        self.ranges = []
        self.shapes = []
        self.location = float(rig.pose.bones['tongue04(drv)'].location[1])
        specs = (
            ('facs_css_TongueTipSide', 'pose.bones["tongue04(drv)"].rotation_euler', 2, 'SUM', None),
            ('facs_css_TongueTipOut', 'pose.bones["tongue04(drv)"].location', 1, None, None),
            ('facs_css_TongueFlatten', 'pose.bones["tongue03(drv)"].scale', 2, 'SCRIPTED', '1+a'),
        )
        for name, channel, index, expected_type, expression in specs:
            curve = rig.animation_data.drivers.find(channel, index=index)
            if name in rig or (curve is None) != (expected_type is None):
                raise ValueError('Unexpected source tongue driver: '+name)
            if curve is not None:
                driver = curve.driver
                if (driver.type != expected_type or
                        (expression is not None and driver.expression != expression) or
                        len(driver.variables) != 1 or len(curve.modifiers) or
                        [tuple(k.co) for k in curve.keyframe_points] != [(0., 0.), (1., 1.)]):
                    raise ValueError('Tongue driver differs from the source audit: '+name)
                target = driver.variables[0].targets[0]
                expected_path = '["tongue04:Rot:2:01"]' if name.endswith('Side') else '["tongue03:Sca:2"]'
                if target.id != rig.data or target.data_path != expected_path:
                    raise ValueError('Tongue driver target differs: '+name)
        for name, channel, index, expected_type, _ in specs:
            rig[name] = 0.0
            created = expected_type is None
            curve = rig.driver_add(channel, index) if created else rig.animation_data.drivers.find(channel, index=index)
            driver = curve.driver
            previous_type, previous_expression = driver.type, driver.expression
            variable = driver.variables.new()
            variable.name = 'css_bake'
            variable.type = 'SINGLE_PROP'
            variable.targets[0].id = rig
            variable.targets[0].data_path = '['+json.dumps(name)+']'
            if name.endswith('Out'):
                driver.type = 'SCRIPTED'
                driver.expression = '-0.01*css_bake'
            elif name.endswith('Flatten'):
                driver.expression = '(1+a)*(1-0.2*css_bake)'
            self.drivers.append((name, curve, variable, created, previous_type, previous_expression))

    def expand_ranges(self, deform, parts):
        for data in (self.rig.data, deform.data):
            for name in data.keys():
                if not name.startswith('facs_') or not name.endswith('(fin)'):
                    continue
                ui = data.id_properties_ui(name)
                prior = ui.as_dict()
                if 'min' not in prior or 'max' not in prior:
                    continue
                self.ranges.append((data, name, prior))
                ui.update(min=min(prior['min'], -.5), max=max(prior['max'], 1.5))
            data.update_tag()
        for obj in parts:
            if obj.data.shape_keys:
                for key in obj.data.shape_keys.key_blocks:
                    if key.name.startswith('facs_'):
                        self.shapes.append((obj, key, key.slider_min, key.slider_max))
                        key.slider_min = min(key.slider_min, -.5)
                        key.slider_max = max(key.slider_max, 1.5)
                obj.data.shape_keys.update_tag()

    def restore(self):
        for data, name, prior in self.ranges:
            data.id_properties_ui(name).update(**prior)
            data.update_tag()
        for obj, key, minimum, maximum in self.shapes:
            key.slider_min, key.slider_max = minimum, maximum
            obj.data.shape_keys.update_tag()
        for _, curve, variable, created, previous_type, previous_expression in self.drivers:
            if created:
                self.rig.driver_remove(curve.data_path, curve.array_index)
            else:
                curve.driver.variables.remove(variable)
                curve.driver.type = previous_type
                curve.driver.expression = previous_expression
        self.rig.pose.bones['tongue04(drv)'].location[1] = self.location

    def remove_properties(self):
        for name, *_ in self.drivers:
            del self.rig[name]

    def audit(self):
        return dict(controls=[row[0] for row in self.drivers],
                    property_ranges=[dict(armature=data.name, name=name, original=prior)
                                     for data, name, prior in self.ranges],
                    shape_ranges=[dict(object=obj.name, name=key.name, minimum=minimum, maximum=maximum)
                                  for obj, key, minimum, maximum in self.shapes])
