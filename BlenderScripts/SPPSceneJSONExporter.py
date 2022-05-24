# Copyright (c) David Sleeper (Sleeping Robot LLC)
# Distributed under MIT license, or public domain if desired and
# recognized in your jurisdiction.

bl_info = {
    "name": "Export SPP Scene JSON (.spj)",
    "author": "David Sleeper (Sleeping Robot LLC)",
    "version": (1, 0, 0),
    "blender": (2, 80, 0),
    "location": "File > Export > SPP Scene Export (.spj)",
    "description": "Export SPP Scene Export (.spj)",
    "warning": "",
    "doc_url": "https://github.com/dsleep/SPP",
    "category": "Import-Export",
}

"""
Related links:
https://github.com/dsleep/SPP

Usage Notes:


"""


import bpy
import bmesh
import struct 
import ctypes
import json
import os
import time

from pathlib import Path

from bpy.props import (
    BoolProperty,
    FloatProperty,
    StringProperty,
    EnumProperty,
    IntProperty,
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


def triangulate_mesh(curMesh):
    # Get a BMesh representation
    newMesh = bmesh.new()
    newMesh.from_mesh(curMesh)

    bmesh.ops.triangulate(newMesh, faces=newMesh.faces[:], quad_method='BEAUTY', ngon_method='BEAUTY' )

    newMesh.to_mesh(curMesh)
    newMesh.free()  


def exportMesh(curMesh, RootPath):

    triangulate_mesh(curMesh)
    
    # this works only in object mode,
    verts = curMesh.vertices
    uv_layer = curMesh.uv_layers.active.data
    curMesh.calc_loop_triangles()  
    # tangents have to be pre-calculated
    # this will also calculate loop normal
    curMesh.calc_tangents()
            
    fileVertTypes = 0b00000001 | 0b00000010 | 0b00000100 | 0b00001000 #has position, UV, normal, tangent
            
    relFileName = curMesh.name + ".bin"
    # if hasattr(curMesh, 'vertex_colors'):
    # if curMesh.vertex_colors:
        # color_layer = curMesh.vertex_colors["Col"]
        # fileVertTypes |= 0b000010000 #and color
    
    with open( os.path.join(RootPath, relFileName),'wb') as f:       # write file in binary mode
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
    

def exportImage(curImage, RootPath):

    orgFilePath = curImage.filepath
    filePathStem = os.path.basename(curImage.filepath) 
    
    writeFilePath = os.path.join(RootPath, curImage.name)
    
    print("Saving...", curImage.name, curImage.file_format, writeFilePath)
    
    try:
        curImage.save_render(writeFilePath)
    except:
        print("An exception occurred")

def WalkUpNodes(InCurNode, FuncNotTraverse):
    FuncNotTraverse(InCurNode)

    if hasattr(InCurNode, 'inputs') :
        for curInput in InCurNode.inputs:
            for curLink in curInput.links:
                if curLink and curLink.from_socket:
                    WalkUpNodes(curLink.from_socket.node, FuncNotTraverse)
                    
def WalkNodesForTextures(InSocket, oReferencedTextures, textureChannel, InGlobalTextureSet):
        
    InCurNode = InSocket.node
    if InCurNode and InCurNode.type == 'TEX_IMAGE':
        print("FOUND IMAGE: ", textureChannel, InCurNode.image)
        oReferencedTextures[textureChannel].append(InCurNode.image.name)
        InGlobalTextureSet.add(InCurNode.image)
        
    for curLink in InSocket.links:
        if curLink and curLink.from_socket and curLink.from_socket != InSocket and curLink.from_socket.node:
            linkedNode = curLink.from_socket.node;
            for curInput in linkedNode.inputs:
                WalkNodesForTextures(curInput, oReferencedTextures, textureChannel, InGlobalTextureSet)
    
def Walk_BSDF_DIFFUSE(InMaterialNode, oReferencedTextures, InGlobalTextureSet):
    print("Walk_BSDF_DIFFUSE")
    
    WalkNodesForTextures( InMaterialNode.inputs["Color"], oReferencedTextures, "diffuse", InGlobalTextureSet )
    WalkNodesForTextures( InMaterialNode.inputs["Normal"], oReferencedTextures, "normal", InGlobalTextureSet )
        
        
def Walk_BSDF_PRINCIPLED(InMaterialNode, oReferencedTextures, InGlobalTextureSet):
    print("Walk_BSDF_PRINCIPLED")
    
    WalkNodesForTextures( InMaterialNode.inputs["Base Color"], oReferencedTextures, "diffuse", InGlobalTextureSet )
    WalkNodesForTextures( InMaterialNode.inputs["Metallic"], oReferencedTextures, "metallic", InGlobalTextureSet )
    WalkNodesForTextures( InMaterialNode.inputs["Specular"], oReferencedTextures, "specular", InGlobalTextureSet )
    WalkNodesForTextures( InMaterialNode.inputs["Roughness"], oReferencedTextures, "roughness", InGlobalTextureSet )
    WalkNodesForTextures( InMaterialNode.inputs["Alpha"], oReferencedTextures, "alpha", InGlobalTextureSet )    
    WalkNodesForTextures( InMaterialNode.inputs["Normal"], oReferencedTextures, "normal", InGlobalTextureSet )    
    
def WalkMaterial(InMaterial,InGlobalTextureSet):
        
    ReferencedTextures = { 
        "diffuse" : [],
        "emissive" : [],
        "alpha" : [],
        "specular" : [],
        "metallic" : [],
        "roughness" : [],
        "normal" : [],
    }
    
    def CheckForBSDNode(InCurNode):
        if InCurNode.type == 'BSDF_DIFFUSE' :
            print("FOUND BSDF_DIFFUSE")
            Walk_BSDF_DIFFUSE(InCurNode, ReferencedTextures, InGlobalTextureSet)
            return
        
        if InCurNode.type == 'BSDF_PRINCIPLED' :
            print("FOUND BSDF_PRINCIPLED")
            Walk_BSDF_PRINCIPLED(InCurNode, ReferencedTextures, InGlobalTextureSet)
            return

    if InMaterial.node_tree:
        matNodes = InMaterial.node_tree.nodes
        
        for curNode in matNodes:
            #print(curNode, curNode.type)
            if curNode.type == 'OUTPUT_MATERIAL':
                print("Found output shader node, walking...", InMaterial)
                
                WalkUpNodes(curNode, CheckForBSDNode)
                
                
    return ReferencedTextures
                
def ExportMaterial(InMat, InGlobalTextureSet, oJson):
    print("")
     
    ReferencedTextures = WalkMaterial(InMat,InGlobalTextureSet)
    
    outMaterial = { 
        "name": InMat.name,
        "textures" : ReferencedTextures,
    }

    oJson["materials"].append(outMaterial)




def do_export(context, props, filepath):

    print(bpy.data.objects)
    print(10)

    print(list(bpy.data.objects))
    
    rootOfWrite = os.path.dirname(filepath)
    print("Writing Files To: ", rootOfWrite)
    
    rootSceneName = Path(bpy.data.filepath).stem
     
    outJson = { 
        "lights": [],
        "meshes" : [], 
        "materials" : []
    }
    
    allMaterials = set()
    allMeshes = set()

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
                    "transform": transformJson
                }
                outJson["lights"].append(newLight)
                
        if obj.type == 'MESH': 
            print("---")
            
            curMesh = obj.data
            
            if curMesh == None or hasattr(curMesh, 'vertices') == False:
                continue;
                
            allMeshes.add(curMesh)
            
            localJsonMats = []
            
            for curMat in obj.material_slots:
                if curMat and curMat.material:
                    allMaterials.add(curMat.material)
                    localJsonMats.append(curMat.material.name)
                else:
                    localJsonMats.append("NONE")
                    
            newMesh = {
                "name" : obj.name,
                "transform": transformJson,
                "mesh" : curMesh.name,
                "materials" : list(localJsonMats)
            }
            outJson["meshes"].append(newMesh)
                            
            
    print("Found meshes! total:", len(allMeshes))    
    for curMesh in allMeshes:
        exportMesh(curMesh, rootOfWrite)                    
             
    allTextures = set()

    print("Found materials! total:", len(allMaterials))    
    for curMat in allMaterials:
        ExportMaterial(curMat,allTextures,outJson)

    print("Found textures! total:", len(allTextures))    
    for curTexture in allTextures:
        exportImage(curTexture, rootOfWrite)  
                
    with open(filepath, "w") as text_file:
        text_file.write(json.dumps(outJson,indent=4, sort_keys=True))
        
    return True


# EXPORT OPERATOR
class Export_SPJ(bpy.types.Operator, ExportHelper):
    """Export the scene as a .spj file"""
    bl_idname = "export_shape.spj"
    bl_label = "Export SPP JSON (.spj)"

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
        
    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        start_time = time.time()
        print('\n_____START_____')
        props = self.properties
        filepath = self.filepath
        filepath = bpy.path.ensure_ext(filepath, self.filename_ext)

        exported = do_export(context, props, filepath)

        if exported:
            print('finished export in %s seconds' %
                  ((time.time() - start_time)))
            print(filepath)

        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.window_manager

        if True:
            # File selector
            wm.fileselect_add(self)  # will run self.execute()
            return {'RUNNING_MODAL'}
        elif True:
            # search the enum
            wm.invoke_search_popup(self)
            return {'RUNNING_MODAL'}
        elif False:
            # Redo popup
            return wm.invoke_props_popup(self, event)
        elif False:
            return self.execute(context)


def menu_func_export_button(self, context):
    self.layout.operator(Export_SPJ.bl_idname, text="SPP JSON (.spj)")


classes = [
    Export_SPJ,
]


def register():
    for cls in classes:
        bpy.utils.register_class(cls)

    bpy.types.TOPBAR_MT_file_export.append(menu_func_export_button)


def unregister():
    bpy.types.TOPBAR_MT_file_export.remove(menu_func_export_button)
    
    for cls in classes:
        bpy.utils.unregister_class(cls)


if __name__ == "__main__":
    register()
