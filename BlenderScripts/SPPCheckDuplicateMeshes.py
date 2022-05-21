# Copyright (c) David Sleeper (Sleeping Robot LLC)
# Distributed under MIT license, or public domain if desired and
# recognized in your jurisdiction.

bl_info = {
    "name": "SPP Check Mesh Duplicates",
    "description": "Check for Duplicate Meshes",
    "author": "Sleeping Robot LLC",
    "version": (1, 0),
    "blender": (2, 80, 0),
    "location": "File > Clean Up > SPP Check Duplicates",
    "warning": "",
    "wiki_url": "",
    "tracker_url": "https://github.com/dsleep/SPP",
    "doc_url": "{BLENDER_MANUAL_URL}/addons/import_export/scene_dxf.html",
    "support": 'COMMUNITY',
    "category": "Mesh"
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

def checkMeshDupesAndClean(context):
    print("checkMeshDupesAndClean")
    meshHashDict = {
    }

    meshHasSets = {
    }

    print("checking meshes:", len( bpy.data.meshes ) )
    for obj in bpy.data.meshes: 
        curMesh = obj        
        curMesh.calc_loop_triangles()  
        
        meshAsJson = {
        }
        
        meshAsJson["vertCount"] = len(curMesh.loop_triangles)
        meshAsJson["triCount"] = len(curMesh.vertices)
        meshAsJson["vertHash"] = 0;
        
        for tri in curMesh.loop_triangles:             
            for loop_index in (tri.loops):
                curLoop = curMesh.loops[loop_index]
                curVertex = curMesh.vertices[curLoop.vertex_index]
                meshAsJson["vertHash"] ^= hash(curVertex.co[0])
                meshAsJson["vertHash"] ^= hash(curVertex.co[1])
                meshAsJson["vertHash"] ^= hash(curVertex.co[2])
        
        meshHash = hash(json.dumps(meshAsJson))
        
        #print("TEST", meshHash, meshAsJson["vertHash"])
        
        meshHashDict[curMesh] = meshHash
        meshHasSets[meshHash] = curMesh
         
    print("Total Meshes:", len(meshHashDict))
    print("Total Hashed Meshes:", len(meshHasSets))
        
    if len(meshHashDict) != len(meshHasSets):
        for obj in bpy.context.scene.objects: 
            print(obj.name, obj, obj.mode, obj.type)
            
            if obj.type == 'MESH': 
                
                curMesh = obj.data
                if curMesh in meshHashDict:
                        
                    curMeshHash = meshHashDict[curMesh]
                    meshToSet = meshHasSets[curMeshHash]
                    
                    if curMesh != meshToSet:
                        print("Swap Mesh: ", curMesh, meshToSet)
                        obj.data = meshToSet
                        
        bpy.data.orphans_purge(do_recursive = True)
                

class ObjectCheckDupes(bpy.types.Operator):    
    bl_idname = "cleanup.sppcheckmeshes"
    bl_label = "SPP Check Duplicate Meshes"
    bl_description = """Check Duplicate Meshes"""
   
    def execute(self, context):
        checkMeshDupesAndClean(context);        
        return {'FINISHED'}


# Add trigger into a dynamic menu
def menu_func_cleanup(self, context):
    self.layout.operator(ObjectCheckDupes.bl_idname, text="SPP Check Duplicate Meshes")
    

def register():
    bpy.utils.register_class(ObjectCheckDupes)
    bpy.types.TOPBAR_MT_file_cleanup.append(menu_func_cleanup)


def unregister():
    bpy.utils.unregister_class(ObjectCheckDupes)
    bpy.types.TOPBAR_MT_file_cleanup.remove(menu_func_cleanup)


if __name__ == "__main__":
    register()