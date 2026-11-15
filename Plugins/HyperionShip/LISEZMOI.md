# Hypérion — plugin Unreal Engine 5

Plugin C++ qui anime le vaisseau **Hypérion (classe Atlas)** à partir de la maquette `Hyperion_Blockout.glb` (fournie dans `SourceArt/`).
La géométrie reste celle du fichier importé : le code retrouve les pièces par leur nom et les anime.

| Fonction | Ce que fait le code |
|---|---|
| Anneau gravitationnel | Rotation continue autour de l'axe du vaisseau, 10,36 °/s (1,7 tr/min, 1 g à 300 m), avec multiplicateur pour la mise en scène |
| Tourelles | Arme principale 5 canons, 8 tourelles lourdes, 4 lasers de poupe : visée vers un point, débattements et vitesses de rotation par famille |
| Tir | 6 groupes (canon axial, arme principale, tourelles lourdes, lasers, VLS, défense rapprochée), cadence par groupe, projectile optionnel, événement `OnWeaponFired` |
| Moteurs | 2 principaux + 6 auxiliaires : poussée lissée, émissif des tuyères, jets Niagara optionnels |
| Propulseurs de manœuvre | 4 à l'avant + 8 à l'arrière : allumage individuel ou 8 manœuvres pré-câblées (tangage, lacet, translations) |
| Antenne parabolique | Balayage lent ou suivi d'une cible |
| Hangars | Ouverture / fermeture des 2 portes (glissement) |
| Balise | Feu clignotant au sommet de l'antenne verticale |

## 1. Installation

Prérequis : Unreal Engine **5.3 ou plus récent**, projet C++ (n'importe quel projet devient C++ en ajoutant une classe C++ vide).

1. Copiez le dossier `HyperionShip` dans `<VotreProjet>/Plugins/`.
2. Clic droit sur le `.uproject` → *Generate Visual Studio project files*, puis compilez (ou lancez l'éditeur, qui proposera de compiler).
3. Vérifiez dans *Edit → Plugins* que **Hyperion Ship** et **Niagara** sont actifs.

## 2. Import de la maquette

1. *File → Import Into Level…* → choisissez `SourceArt/Hyperion_Blockout.glb`, dossier de destination `/Game/Hyperion`.
   L'import crée un acteur par pièce, avec la même hiérarchie et les mêmes noms que dans le fichier (`BP_Hyperion`, `SM_S1_Proue`, `Pivot_Anneau`, `WPN_Heavy_01_Lacet`…).
2. Vérifiez l'orientation : la proue doit pointer vers **+X**, le dessus vers **+Z**. Si ce n'est pas le cas, tournez l'acteur racine `BP_Hyperion` — le code calcule de toute façon l'axe du vaisseau à partir des pièces.
3. *(Facultatif)* *Tools → Execute Python Script…* → `Tools/hyperion_setup.py` (plugin *Python Editor Script* requis). Le script active Nanite sur les meshes importées et liste les pièces manquantes dans le journal.

## 3. Création du Blueprint

1. Dans l'Outliner, sélectionnez `BP_Hyperion` puis clic droit → *Select → Select All Descendants* (ou sélectionnez toute la hiérarchie).
2. *Blueprints → Convert Selection to Blueprint Class…*
   - mode : **Harvest Components** ;
   - classe parente : **Hyperion Ship** ;
   - nom : `BP_HyperionShip`, dossier `/Game/Hyperion`.
3. Ouvrez le Blueprint et passez le composant racine en **Movable** (le code le fait aussi au démarrage, mais l'éditeur évite ainsi les avertissements d'éclairage statique).
4. Supprimez les acteurs importés du niveau et placez `BP_HyperionShip` à la place.

Au lancement, le journal affiche « N tourelles, 8 moteurs, 12 propulseurs de manœuvre ». Les pièces introuvables y sont listées (`GetMissingPartCount()` doit valoir 0).

> Les composants doivent garder **exactement** les noms des nœuds du fichier. Si vous renommez une pièce, le code ne la retrouvera plus.

## 4. Réglages (Class Defaults du Blueprint)

| Catégorie | Paramètre | Rôle |
|---|---|---|
| Hyperion › Anneau | `RingSpeedDegPerSec`, `RingTimeScale` | Vitesse réelle (10,36 °/s) et multiplicateur (0 = arrêt) |
| Hyperion › Propulsion | `MainExhaustFX`, `AuxExhaustFX`, `RCSFX` | Systèmes Niagara des jets (paramètre float `User.Thrust`, 0 à 1) |
| | `GlowMaterialParam` | Paramètre vectoriel d'émissif du matériau `M_Propulsion` importé (par défaut `EmissiveFactor`) |
| | `EngineGlowColor`, `GlowAtFullThrottle`, `ThrottleResponse` | Couleur, intensité et réactivité des tuyères |
| Hyperion › Armement | `ProjectileClasses` | Acteur projectile par groupe d'armes (vide = événement seulement) |
| | `FireIntervals` | Cadence par groupe (s) : canon axial 20, principale 6, lourdes 2, lasers 0,5, VLS 4, CIWS 0,1 |
| | `HeavyLimits`, `MainLimits`, `LaserLimits` | Débattements en lacet et en site, vitesse de rotation |
| | `AimTolerance` | Écart maximal (°) pour qu'une tourelle tire |
| Hyperion › Antenne | `bDishSweep`, `DishTurnRate` | Balayage automatique et vitesse |
| Hyperion › Hangars | `DoorSlideCm`, `DoorSpeed` | Course et vitesse des portes |
| Hyperion › Balise | `bBeacon`, `BeaconColor`, `BeaconPeriod` | Feu clignotant |

## 5. Fonctions Blueprint

| Fonction | Usage |
|---|---|
| `SetThrottle(Main, Aux)` | Poussée 0 à 1 des moteurs principaux et auxiliaires (montée progressive) |
| `PerformManeuver(Manoeuvre, Durée)` | Cabrer, piquer, lacet gauche/droite, translations haut/bas/gauche/droite |
| `FireRCS(Nom, Durée)` | Allume un propulseur précis, ex. `SM_Propulseur_Manoeuvre_AR_G1` |
| `AimAllTurrets(Cible)` / `AimGroup(Groupe, Cible)` | Oriente les tourelles vers un point du monde |
| `StowTurrets()` | Remet les tourelles au repos |
| `FireWeapon(Groupe, Cible)` | Tire ; seules les tourelles en joue et dans leur secteur partent. Renvoie le nombre de départs |
| `OnWeaponFired` (événement) | Reçoit le groupe et la position/orientation de chaque bouche : idéal pour flashs, sons, recul |
| `SetHangarDoorsOpen(Ouvert)` | Ouvre ou ferme les deux hangars |
| `SetDishTarget(Cible)` / `ClearDishTarget()` | Suivi d'une cible par l'antenne / retour au balayage |
| `GetShipForward()` | Axe du vaisseau (vers la proue) en coordonnées monde |
| `FindPart(Nom)` | Accès direct à n'importe quelle pièce de la maquette |

Exemple minimal : *Event BeginPlay* → `SetThrottle(0.3, 0.1)` → `SetHangarDoorsOpen(true)` ; *Event Tick* → `AimAllTurrets(position de la cible)` → `FireWeapon(Heavy, position de la cible)`.

## 6. Conventions et limites

- Unités : 1 unité Unreal = 1 cm. Le vaisseau mesure environ 125 000 unités : gardez-le près de l'origine pour les cinématiques.
- Repères locaux supposés après import : canons selon **+X** de la pièce *Site*, lacet autour de **+Z** de l'embase, tuyères des propulseurs de manœuvre selon **+Z** local. Les tourelles ventrales et les lasers de poupe sont déjà retournés dans le fichier, le code n'a rien à corriger. L'axe de l'anneau est calculé automatiquement.
- Le plugin ne contient pas de modèle de vol : `PerformManeuver` et `SetThrottle` gèrent l'aspect visuel. Branchez votre propre physique (ou un `FloatingPawnMovement`) en parallèle.
- Les noms de paramètres Niagara et matériau sont réglables, car ils dépendent de vos effets et de la version d'Interchange.
