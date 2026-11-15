"""Hyperion - preparation des assets apres import (Editeur Unreal, menu Outils > Executer un script Python).

- active Nanite sur toutes les static meshes importees depuis Hyperion_Blockout.glb ;
- verifie que les pieces attendues par AHyperionShip existent bien parmi les acteurs du niveau.

Reglez ASSET_FOLDER sur le dossier ou vous avez importe le fichier .glb.
"""
import unreal

ASSET_FOLDER = "/Game/Hyperion"

EXPECTED = (
    ["Pivot_Anneau", "SM_Anneau", "SM_Rayons", "WPN_Spinal", "WPN_Main", "WPN_Main_Lacet", "WPN_Main_Site",
     "SM_Antenne_Parabole", "SM_Antenne_Passerelle", "SM_PorteHangar_G", "SM_PorteHangar_D",
     "SM_Tuyere_Main_L", "SM_Tuyere_Main_R"]
    + [f"SM_Tuyere_Aux_{i:02d}" for i in range(1, 7)]
    + [f"WPN_Heavy_{i:02d}{s}" for i in range(1, 9) for s in ("", "_Lacet", "_Site")]
    + [f"WPN_Laser_{i:02d}{s}" for i in range(1, 5) for s in ("", "_Site")]
    + [f"VLS_{i:02d}" for i in range(1, 5)]
    + [f"CIWS_{i:02d}" for i in range(1, 24)]
    + [f"SM_Propulseur_Manoeuvre_{f}" for f in "HBGD"]
    + [f"SM_Propulseur_Manoeuvre_AR_{f}{k}" for f in "HBGD" for k in (1, 2)]
)


def enable_nanite():
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    count = 0
    for path in unreal.EditorAssetLibrary.list_assets(ASSET_FOLDER, recursive=True, include_folder=False):
        asset = unreal.EditorAssetLibrary.load_asset(path)
        if not isinstance(asset, unreal.StaticMesh):
            continue
        settings = asset.get_editor_property("nanite_settings")
        if settings.get_editor_property("enabled"):
            continue
        settings.set_editor_property("enabled", True)
        try:
            subsystem.set_nanite_settings(asset, settings, apply_changes=True)
        except AttributeError:
            asset.set_editor_property("nanite_settings", settings)
        unreal.EditorAssetLibrary.save_loaded_asset(asset)
        count += 1
    unreal.log(f"[Hyperion] Nanite active sur {count} mesh(es).")


def check_level_parts():
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    labels = {a.get_actor_label() for a in actors}
    missing = [n for n in EXPECTED if n not in labels]
    if missing:
        unreal.log_warning(f"[Hyperion] {len(missing)} piece(s) absente(s) du niveau : {', '.join(missing)}")
    else:
        unreal.log(f"[Hyperion] Les {len(EXPECTED)} pieces animees sont presentes dans le niveau.")


enable_nanite()
check_level_parts()
