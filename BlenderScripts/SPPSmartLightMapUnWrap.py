import bpy

#print(dir(bpy.types)) # VIEW3D_MT_object.append(custom_draw) # Object menu

class SPP_SmartLightMapUnWrap(bpy.types.Operator):
    bl_idname  = "spp.smartlightmapunwrap"
    bl_label = "SPP Simple Smart Unwrapper with default settings..."

    def execute(self, context):
        # Get all objects in selection
        selection = bpy.context.selected_objects

        # Get the active object
        active_object = bpy.context.active_object

        # Deselect all objects
        bpy.ops.object.select_all(action='DESELECT')

        for obj in selection:            
            if obj.type == 'MESH':
                
                if "UVMap_LightMap" not in obj.data.uv_layers.keys():
                    obj.data.uv_layers.new(name="UVMap_LightMap")
                    
                # Select each object
                obj.select_set(True)
                # Make it active
                bpy.context.view_layer.objects.active = obj
                # Toggle into Edit Mode
                bpy.ops.object.mode_set(mode='EDIT')
                # Select the geometry
                bpy.ops.mesh.select_all(action='SELECT')
                # Call the smart project operator
                obj.data.uv_layers.active = obj.data.uv_layers["UVMap_LightMap"]
                bpy.ops.uv.smart_project(island_margin=0.001, area_weight=1.0, correct_aspect=True, scale_to_bounds=True)
                # Toggle out of Edit Mode
                bpy.ops.object.mode_set(mode='OBJECT')
                # Deselect the object
                obj.select_set(False)

        # Restore the selection
        for obj in selection:
            obj.select_set(True)

        # Restore the active object
        bpy.context.view_layer.objects.active = active_object

        return {'FINISHED'}    

def draw_menu(self, context):
    layout = self.layout
    layout.separator()
    layout.operator(SPP_SmartLightMapUnWrap.bl_idname, text="SPP: Light Map Smart Unwrap")

def register():
    bpy.utils.register_class(SPP_SmartLightMapUnWrap)
    bpy.types.VIEW3D_MT_object_context_menu.append(draw_menu)

def unregister():
    bpy.utils.unregister_class(SPP_SmartLightMapUnWrap)
    bpy.types.VIEW3D_MT_object_context_menu.remove(draw_menu)
    
if __name__ == "__main__":
    register()