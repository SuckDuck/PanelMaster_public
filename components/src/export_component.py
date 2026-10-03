import bpy
import json
import os
import re


class EXPORT_OT_mwc(bpy.types.Operator):
    bl_idname = "export.mwc"
    bl_label = "Export MWC (PanelMaster Component)"
    filename_ext = ".mwc"
    filepath: bpy.props.StringProperty(subtype="FILE_PATH")
    extensible: bpy.props.BoolProperty(
        name="Extensible",
        description="Export component as extensible",
        default=False
    )

    def check(self, context):
        if self.filepath:
            base, ext = os.path.splitext(self.filepath)
            if ext.lower() != ".mwc":
                self.filepath = base + ".mwc"
        return True

    # -------------------------
    # FIND SIZE SOURCE
    # -------------------------
    def get_size(self, obj):
        # Only use global object named "size"
        for o in bpy.data.objects:
            if o.type == 'MESH' and o.name.lower() == "size":
                return abs(o.dimensions.x), abs(o.dimensions.y)

        # fallback
        return 1.0, 1.0
    
    def get_lods(self):
        lods_unsorted = []

        for o in bpy.data.objects:
            if o.type == 'MESH' and o.name.startswith("LOD"):
                match = re.match(r"LOD(\d+)", o.name)
                if match:
                    index = int(match.group(1))
                    lods_unsorted.append((index, o))

        if not lods_unsorted:
            return None

        lods = [obj for index, obj in sorted(lods_unsorted, key=lambda x: x[0])]
        return lods
    
    def get_cutout(self):
        cutout = None
        for o in bpy.data.objects:
            if o.type == 'MESH' and o.name.lower() == "cutout":
                cutout = o

        if not cutout:
            return None
        
        points = [] 
        for v in cutout.data.vertices: 
            world = cutout.matrix_world @ v.co 
            points.append({ 
                "x": world.x, 
                "y": world.y 
            }) 
            
        return points

    def execute(self, context):
        lods = self.get_lods()

        # -------------------------
        # NAME
        # -------------------------
        filepath = self.filepath
        if not filepath.lower().endswith(".mwc"):
            filepath += ".mwc"

        name = os.path.splitext(os.path.basename(filepath))[0]

        # -------------------------
        # WIDTH / HEIGHT
        # -------------------------
        width, height = self.get_size(lods[0])

        # -------------------------
        # POINTS
        # -------------------------
        points = []
        for v in lods[0].data.vertices:
            world = lods[0].matrix_world @ v.co
            points.append({
                "x": world.x,
                "y": world.y
            })

        # -------------------------
        # LINES
        # -------------------------
        lines = []
        for lod in lods:
            lod_lines = []
            for e in lod.data.edges:
                lod_lines.append({
                    "A": e.vertices[0],
                    "B": e.vertices[1]
                })

            name_parts = lod.name.split("_")
            zoom = 1.0
            if len(name_parts) > 1:
                try: zoom = float(name_parts[1])
                except: zoom = 1.0

            lines.append({
                "zoom": zoom,
                "lines": lod_lines
            })

        # -------------------------
        # OUTPUT STRUCTURE
        # -------------------------
        output = {
            "version": 0,
            "type": 1 if self.extensible else 0,
            "id": name,
        }

        if self.extensible:
            output["width"] = height            

        else:
            output["width"] = width
            output["height"] = height
            
        output["skin"] = {
            "points": points,
            "LOD": lines        
        }
        
        cutout = self.get_cutout()
        if cutout: output["cutout"] = cutout

        # -------------------------
        # WRITE FILE
        # -------------------------
        try:
            with open(filepath, "w", encoding="utf-8") as f:
                json.dump(output, f, indent=4)
        except Exception as e:
            self.report({'ERROR'}, f"Failed to write file: {e}")
            return {'CANCELLED'}

        self.report({'INFO'}, f"Exported MWC to {filepath}")
        print("Export completed:", filepath)

        return {'FINISHED'}

    def invoke(self, context, event):
        obj = context.active_object

        if obj:
            self.filepath = bpy.path.ensure_ext(obj.name, ".mwc")

        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}


def menu_func_export(self, context):
    self.layout.operator(
        EXPORT_OT_mwc.bl_idname,
        text="Export MWC (PanelMaster Component)"
    )


def register():
    bpy.utils.register_class(EXPORT_OT_mwc)
    bpy.types.TOPBAR_MT_file_export.append(menu_func_export)


def unregister():
    bpy.types.TOPBAR_MT_file_export.remove(menu_func_export)
    bpy.utils.unregister_class(EXPORT_OT_mwc)


if __name__ == "__main__":
    register()