# Copyright (c) David Sleeper (Sleeping Robot LLC)
# Distributed under MIT license, or public domain if desired and
# recognized in your jurisdiction.

bl_info = {
    "name": "SPP Format Exporter",
    "description": "Export SPP Simple Scenes",
    "author": "Sleeping Robot LLC",
    "version": (1, 0),
    "blender": (2, 80, 0),
    "location": "File > Export > SPP Export",
    "warning": "",
    "wiki_url": "",
    "tracker_url": "https://github.com/dsleep/SPP",
    "doc_url": "{BLENDER_MANUAL_URL}/addons/import_export/scene_dxf.html",
    "support": 'COMMUNITY',
    "category": "Import-Export"
}

import bpy
import bmesh
import struct 
import ctypes
import json
import os
from pathlib import Path

from bpy.props import (
    BoolProperty,
    FloatProperty,
    StringProperty,
    EnumProperty,
)
from bpy_extras.io_utils import (
    ImportHelper,
    ExportHelper,
    orientation_helper,
    path_reference_mode,
    axis_conversion,
)
from bpy.types import (
    Operator,
    OperatorFileListElement,
)

from bpy import context

def writeFiles(context, props):
    print(bpy.data.objects)
    print(10)

    print(list(bpy.data.objects))
    
    rootOfWrite = os.path.dirname(props.filepath)
    print("Writing Files To: ", rootOfWrite)
    
    rootSceneName = Path(bpy.data.filepath).stem
     
    outJson = { 
        "lights": [],
        "meshes" : [] 
    }

    for obj in bpy.context.scene.objects: 
        print(obj.name, obj, obj.mode, obj.type)
        #if obj.type == 'MESH': 
        #print("Has Mesh: ", obj.name)
        print(obj.matrix_world[0])
        
        loc, rot, scale = obj.matrix_world.decompose()
    
        transformJson = { 
            "location" : list(loc),
            "rotationQuat" : list(rot),
            "scale" : list(scale)
        }
        #print(dir(obj))
        #else:
        #    print("Has Mesh: ", obj.name)
        if obj.type == 'LIGHT': 
            
            lightData = obj.data
            #print(dir(lightData))
            
            if lightData.type == 'POINT':
                print(lightData.energy, lightData.color, lightData.distance, lightData.type)
                
                newLight = {
                    "name" : obj.name,
                    "type" : lightData.type,
                    "energy" : lightData.energy,
                    "color" : list(lightData.color),
                    "distance" : lightData.distance,
                    "transform": transformJson,
                }
                outJson["lights"].append(newLight)
                
        if obj.type == 'MESH': 
            print("---")

            curMesh = obj.data
            # this works only in object mode,
            verts = curMesh.vertices
            uv_layer = curMesh.uv_layers.active.data
            curMesh.calc_loop_triangles()  
            # tangents have to be pre-calculated
            # this will also calculate loop normal
            curMesh.calc_tangents()
            
            relFileName = rootSceneName + "." + obj.name + ".bin" 
            
            newMesh = {
                "name" : obj.name,
                "transform": transformJson,
                "relFilePath" : relFileName
            }
            outJson["meshes"].append(newMesh)
                
            fileVertTypes = 0b00000001 | 0b00000010 | 0b00000100 | 0b00001000 #has position, UV, normal, tangent
            
            if curMesh.vertex_colors:
                color_layer = curMesh.vertex_colors["Col"]
                fileVertTypes |= 0b000010000 #and color
            
            with open( os.path.join(rootOfWrite, relFileName),'wb') as f:       # write file in binary mode
                # version 1.0
                f.write(ctypes.c_uint(1)) 
                f.write(ctypes.c_uint(0)) 
                
                f.write(ctypes.c_uint(fileVertTypes))
                
                f.write(ctypes.c_uint(len(curMesh.loop_triangles)*3))   
                print("Writing out verts:", len(curMesh.loop_triangles)*3)
                for tri in curMesh.loop_triangles:
                    
                    for loop_index in (tri.loops):
                        curLoop = curMesh.loops[loop_index]
                        curVertex = curMesh.vertices[curLoop.vertex_index]
                        curUV = uv_layer[loop_index].uv
                        curNormal = curLoop.normal
                        curTangent = curLoop.tangent
                        
                        f.write(ctypes.c_float(curVertex.co[0]))
                        f.write(ctypes.c_float(curVertex.co[1]))
                        f.write(ctypes.c_float(curVertex.co[2]))
                        
                        f.write(ctypes.c_float(uv_layer[loop_index].uv[0]))
                        f.write(ctypes.c_float(uv_layer[loop_index].uv[1]))
                        
                        f.write(ctypes.c_float(curNormal[0]))
                        f.write(ctypes.c_float(curNormal[1]))
                        f.write(ctypes.c_float(curNormal[2]))
                        
                        f.write(ctypes.c_float(curTangent[0]))
                        f.write(ctypes.c_float(curTangent[1]))
                        f.write(ctypes.c_float(curTangent[2]))
                                                
    with open(props.filepath, "w") as text_file:
        text_file.write(json.dumps(outJson,indent=4, sort_keys=True))
                


class ObjectExport(bpy.types.Operator, ExportHelper):
    
    bl_idname = "export_mesh.spj"
    bl_label = "Export SPP JSON"
    bl_description = """Export scene to JSON SPP Formats"""

    filename_ext = ".spj"
    
    filter_glob: StringProperty(
        default="*.spj",
        options={'HIDDEN'},
    )
    
    export_select: BoolProperty(
        name="Selected only",
        description="Export selected items only",
        default=False,)
        
   
    
    use_normals: BoolProperty(
        name="Write Normals",
        description="Export one normal per vertex and per face, to represent flat faces and sharp edges",
        default=True,
    )

    use_mesh_modifiers: BoolProperty(
        name="Apply Modifiers",
        description="Apply the modifiers before saving",
        default=True,
    )

    @property
    def check_extension(self):
        return True
        
    def execute(self, context):
        
        props = self.properties

        writeFiles(context, props);        
        return {'FINISHED'}

    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}



# Add trigger into a dynamic menu
def menu_func_export(self, context):
    self.layout.operator(ObjectExport.bl_idname, text="SPP Scene Export (.spj)")
    

def register():
    bpy.utils.register_class(ObjectExport)
    bpy.types.TOPBAR_MT_file_export.append(menu_func_export)


def unregister():
    bpy.utils.unregister_class(ObjectExport)
    bpy.types.TOPBAR_MT_file_export.remove(menu_func_export)


if __name__ == "__main__":
    register()