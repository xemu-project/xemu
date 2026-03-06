#!/usr/bin/env bash
# steam-deck-pgo-wrapper.sh — PGO profiling wrapper for Steam Game Mode
#
# SETUP (une seule fois, en Desktop Mode) :
#   1. Télécharge l'AppImage instrumentée (artifact "xemu-zen2-pgo-instrumented")
#      depuis GitHub Actions et place-la dans ~/xemu-pgo.AppImage
#   2. chmod +x ~/xemu-pgo.AppImage ~/scripts/steam-deck-pgo-wrapper.sh
#   3. Dans Steam → Ajouter un jeu non-Steam → sélectionne CE script
#   4. Propriétés du jeu → Options de lancement :
#        GITHUB_TOKEN=ghp_xxx XEMU_APPIMAGE=~/xemu-pgo.AppImage %command%
#   5. Lance depuis le Game Mode → les profils s'uploadent automatiquement à la fermeture
#
# VARIABLES D'ENVIRONNEMENT :
#   XEMU_APPIMAGE   Chemin vers l'AppImage instrumentée  [défaut : ~/xemu-pgo.AppImage]
#   GITHUB_TOKEN    Personal Access Token (scope: repo)  [requis pour l'upload]
#   GITHUB_REPO     Dépôt cible                          [défaut : Dmamss/xemu]
#   XEMU_PGO_DIR    Dossier de stockage des profils      [défaut : ~/.local/share/xemu-pgo]

set -euo pipefail

APPIMAGE="${XEMU_APPIMAGE:-$HOME/xemu-pgo.AppImage}"
PROFILE_DIR="${XEMU_PGO_DIR:-$HOME/.local/share/xemu-pgo}"
GITHUB_REPO="${GITHUB_REPO:-Dmamss/xemu}"
GITHUB_TOKEN="${GITHUB_TOKEN:-}"
RELEASE_TAG="pgo-profiles"

# ---------------------------------------------------------------------------
log() { echo "[PGO] $*"; }

die() { echo "[PGO] ERREUR: $*" >&2; exit 1; }

# ---------------------------------------------------------------------------
# Vérifie que l'AppImage existe
[ -f "$APPIMAGE" ] || die "AppImage introuvable : $APPIMAGE"

# Dossier de profils persistant (survit aux reboots, contrairement à /tmp)
mkdir -p "$PROFILE_DIR"

# LLVM_PROFILE_FILE override : %p = PID (évite les collisions si fork)
export LLVM_PROFILE_FILE="$PROFILE_DIR/xemu-%p.profraw"

log "Lancement de xemu (instrumented) depuis $APPIMAGE"
log "Profils → $PROFILE_DIR"

# Lance xemu ; on passe tous les arguments transmis par Steam
"$APPIMAGE" "$@" || true   # on continue même si xemu plante

# ---------------------------------------------------------------------------
log "xemu fermé — collecte des profils..."

PROFRAW_FILES=("$PROFILE_DIR"/*.profraw)
if [ ! -f "${PROFRAW_FILES[0]:-}" ]; then
    log "Aucun fichier .profraw trouvé — xemu n'a peut-être pas tourné assez longtemps."
    exit 0
fi

PROFRAW_COUNT=${#PROFRAW_FILES[@]}
log "$PROFRAW_COUNT fichier(s) .profraw trouvé(s)"

# ---------------------------------------------------------------------------
# Merge avec llvm-profdata (cherche plusieurs noms possibles sur Steam Deck)
PROFDATA="$PROFILE_DIR/xemu-$(date +%Y%m%d-%H%M%S).profdata"

LLVM_PROFDATA=""
for cmd in llvm-profdata llvm-profdata-21 llvm-profdata-20 llvm-profdata-19; do
    if command -v "$cmd" &>/dev/null; then
        LLVM_PROFDATA="$cmd"
        break
    fi
done

if [ -z "$LLVM_PROFDATA" ]; then
    log "llvm-profdata non trouvé — les fichiers .profraw bruts sont conservés dans $PROFILE_DIR"
    log "Installe llvm en Desktop Mode : sudo pacman -S llvm"
    log "Puis relance ce script pour merger et uploader."
    exit 0
fi

log "Merge des profils avec $LLVM_PROFDATA..."
"$LLVM_PROFDATA" merge -output="$PROFDATA" "$PROFILE_DIR"/*.profraw
log "Profils mergés → $PROFDATA"

# Nettoyage des .profraw mergés
rm -f "$PROFILE_DIR"/*.profraw
log "Fichiers .profraw nettoyés"

# ---------------------------------------------------------------------------
# Upload vers GitHub Releases
if [ -z "$GITHUB_TOKEN" ]; then
    log "GITHUB_TOKEN non défini — profdata conservé localement : $PROFDATA"
    log "Pour uploader, relance avec : GITHUB_TOKEN=ghp_xxx $0"
    exit 0
fi

log "Upload vers GitHub ($GITHUB_REPO, release tag: $RELEASE_TAG)..."

ASSET_NAME="$(basename "$PROFDATA")"
GITHUB_API="https://api.github.com"

# Crée la release "pgo-profiles" si elle n'existe pas encore
RELEASE_JSON=$(curl -s -o /dev/null -w "%{http_code}" \
    -H "Authorization: token $GITHUB_TOKEN" \
    -H "Accept: application/vnd.github+json" \
    "$GITHUB_API/repos/$GITHUB_REPO/releases/tags/$RELEASE_TAG")

if [ "$RELEASE_JSON" = "404" ]; then
    log "Création de la release '$RELEASE_TAG'..."
    RELEASE_ID=$(curl -s \
        -X POST \
        -H "Authorization: token $GITHUB_TOKEN" \
        -H "Accept: application/vnd.github+json" \
        -H "Content-Type: application/json" \
        "$GITHUB_API/repos/$GITHUB_REPO/releases" \
        -d "{\"tag_name\":\"$RELEASE_TAG\",\"name\":\"PGO Profiles\",\"body\":\"Profiling data collecté depuis le Steam Deck.\",\"draft\":false,\"prerelease\":true}" \
        | grep -o '"id":[0-9]*' | head -1 | cut -d: -f2)
else
    RELEASE_ID=$(curl -s \
        -H "Authorization: token $GITHUB_TOKEN" \
        -H "Accept: application/vnd.github+json" \
        "$GITHUB_API/repos/$GITHUB_REPO/releases/tags/$RELEASE_TAG" \
        | grep -o '"id":[0-9]*' | head -1 | cut -d: -f2)
fi

[ -n "$RELEASE_ID" ] || die "Impossible de récupérer/créer la release. Vérifie ton GITHUB_TOKEN."

log "Upload de $ASSET_NAME (release ID: $RELEASE_ID)..."
UPLOAD_URL="https://uploads.github.com/repos/$GITHUB_REPO/releases/$RELEASE_ID/assets?name=$ASSET_NAME"

HTTP_STATUS=$(curl -s -o /dev/null -w "%{http_code}" \
    -X POST \
    -H "Authorization: token $GITHUB_TOKEN" \
    -H "Accept: application/vnd.github+json" \
    -H "Content-Type: application/octet-stream" \
    --data-binary @"$PROFDATA" \
    "$UPLOAD_URL")

if [ "$HTTP_STATUS" = "201" ]; then
    log "Upload réussi !"
    log "Profils disponibles sur : https://github.com/$GITHUB_REPO/releases/tag/$RELEASE_TAG"
    rm -f "$PROFDATA"
else
    log "Échec de l'upload (HTTP $HTTP_STATUS) — profdata conservé : $PROFDATA"
fi
