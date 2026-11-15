# Runs inside ParaView (pvpython): TenSense's record reader plugin, and the
# .vtp export, as ParaView reads them.
#
#   pvpython check_record_reader.py <plugin.py> <record folder> <tampered copy> <out.json> <old copy>
#
# 1. The plugin loads and opens <record>/manifest.tsrec by extension: the
#    curve with its arrays, its results and the record's metadata.
# 2. A copy whose curve.vtp was changed after writing gives nothing.
# 3. A copy saved as before the rename to TenSense (manifest.tfrec,
#    "tenforge-record") opens by extension and gives the curve.
import json
import sys

from paraview.simple import LoadPlugin, OpenDataFile
from paraview import servermanager as sm

plugin, record, tampered, out, old = sys.argv[1:6]
result = {"ok": False}
try:
    LoadPlugin(plugin, ns=globals())
    src = OpenDataFile(record + "/manifest.tsrec")
    result["reader"] = src.GetXMLName()
    src.UpdatePipeline()
    data = sm.Fetch(src)
    pd, fd = data.GetPointData(), data.GetFieldData()
    result["points"] = data.GetNumberOfPoints()
    result["point_arrays"] = [pd.GetArrayName(i) for i in range(pd.GetNumberOfArrays())]
    result["Rm_Pa"] = fd.GetArray("Rm_Pa").GetValue(0) if fd.GetArray("Rm_Pa") else None
    strings = {}
    for i in range(fd.GetNumberOfArrays()):
        a = fd.GetAbstractArray(i)
        if a.GetDataTypeAsString() == "string":
            strings[a.GetName()] = a.GetValue(0)
    result["strings"] = strings

    bad = OpenDataFile(tampered + "/manifest.tsrec")
    bad.UpdatePipeline()
    result["tampered_points"] = sm.Fetch(bad).GetNumberOfPoints()

    before = OpenDataFile(old + "/manifest.tfrec")
    result["old_reader"] = before.GetXMLName() if before else None
    before.UpdatePipeline()
    result["old_points"] = sm.Fetch(before).GetNumberOfPoints()

    result["ok"] = (result["reader"] == "TenSenseRecordReader"
                    and result["points"] > 0
                    and "stress_MPa" in result["point_arrays"]
                    and result["Rm_Pa"] is not None
                    and strings.get("mode") == "tension"
                    and strings.get("procedure", "").startswith("iso-6892-1-2019")
                    and result["tampered_points"] == 0
                    and result["old_reader"] == "TenSenseRecordReader"
                    and result["old_points"] == result["points"])
except Exception as e:
    import traceback
    result["error"] = traceback.format_exc()
with open(out, "w") as f:
    json.dump(result, f, indent=2)
print(json.dumps(result, indent=2))
