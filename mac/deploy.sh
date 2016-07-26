#!/bin/sh

DEPLOY_PATH="./tmp"

function deploy_dependency()
{
  local filename=$(basename "${1}")
  local lib_path="$(dirname "${2}")/../Frameworks/"

  local target="$lib_path$filename"

  if [ ! -f "$target" ]; then
    cp "${1}" "$target"
    echo "\tDeployed $filename"
    local new_link="@executable_path/../Frameworks/$filename"
    install_name_tool -id "$new_link" $target
  fi

  local new_link="@executable_path/../Frameworks/$filename"
  install_name_tool -change "${1}" "$new_link" ${2}
}

function deploy_dependencies()
{
  local filename=$(basename "${1}")
  local lib_path="$(dirname "${1}")/../Frameworks/"

  for i in ${1}; do
    dependencies=`otool -L $i | grep -i -v '/usr/lib' | grep -i -v '/System/' | grep -i '.dylib' | cut -d' ' -f 1`
    for d in $dependencies; do
      if [[ "$i:" != "$d" ]] && [[ "$i" != "$d" ]]; then
        deploy_dependency $d ${1}
        if [[ "$filename" != "$(basename $d)" ]]; then
          deploy_dependencies "$lib_path$(basename $d)"
        fi
      fi
    done
  done
}

function create_icon()
{
  echo "\tCreate icon"
  local iconset_path="temp.iconset"
  mkdir ${iconset_path}
  sips -z 16 16     ${1} --out ${iconset_path}/icon_16x16.png
  sips -z 32 32     ${1} --out ${iconset_path}/icon_16x16@2x.png
  sips -z 32 32     ${1} --out ${iconset_path}/icon_32x32.png
  sips -z 64 64     ${1} --out ${iconset_path}/icon_32x32@2x.png
  sips -z 128 128   ${1} --out ${iconset_path}/icon_128x128.png
  sips -z 256 256   ${1} --out ${iconset_path}/icon_128x128@2x.png
  sips -z 256 256   ${1} --out ${iconset_path}/icon_256x256.png
  sips -z 512 512   ${1} --out ${iconset_path}/icon_256x256@2x.png
  sips -z 512 512   ${1} --out ${iconset_path}/icon_512x512.png
  cp ${1} ${iconset_path}/icon_512x512@2x.png
  iconutil -c icns ${iconset_path} --output ${2}
  rm -R ${iconset_path}
}

function create_info_plist()
{
  local major_version=`echo "${VERSION}" | cut -d'.' -f 1`
  local minor_version=`echo "${VERSION}" | cut -d'.' -f 2`
  local bin_name=$(basename ${3})

  echo "Create Info.plist"
  echo '<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple Computer//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleGetInfoString</key>
  <string>'${2}'</string>
  <key>CFBundleExecutable</key>
  <string>'${bin_name}'</string>
  <key>CFBundleIdentifier</key>
  <string>com.freeserf</string>
  <key>CFBundleName</key>
  <string>'${2}'</string>
  <key>CFBundleIconFile</key>
  <string>'${4}'</string>
  <key>CFBundleShortVersionString</key>
  <string>'${VERSION}'</string>
  <key>CFBundleInfoDictionaryVersion</key>
  <string>6.0</string>
  <key>CFBundlePackageType</key>
  <string>APPL</string>
  <key>IFMajorVersion</key>
  <integer>'${major_version}'</integer>
  <key>IFMinorVersion</key>
  <integer>'${minor_version}'</integer>
</dict>
</plist>
  ' > "${1}/Contents/Info.plist"
}

function create_app()
{
  echo "Create application ${1}"
  local deploy_app_path="$DEPLOY_PATH/${1}.app"

  mkdir -p $deploy_app_path/Contents/{MacOS,Resources,Frameworks}

  local icon_name="${1}.icns"
  create_icon "./mac/icon.png" "$deploy_app_path/Contents/Resources/${icon_name}"

  create_info_plist $deploy_app_path ${1} ${2} ${icon_name}

  filename=$(basename $2)
  target="$deploy_app_path/Contents/MacOS/$filename"
  cp ${2} "$target"

  deploy_dependencies $target
}

mkdir -p $DEPLOY_PATH

create_app ${2} ${1}

mkdir -p "${DEPLOY_PATH}/.background"
cp "./mac/background.png" "${DEPLOY_PATH}/.background/background.png"

hdiutil create -srcfolder "${DEPLOY_PATH}" -volname "${2}" -fs HFS+ -fsargs "-c c=64,a=16,e=16" -format UDRW -size 10m ./temp.dmg
#rm -rf ${DEPLOY_PATH}
device=$(hdiutil attach -readwrite -noverify -noautoopen "./temp.dmg" | egrep '^/dev/' | sed 1q | awk '{print $1}')
echo '
   tell application "Finder"
     tell disk "'${2}'"
           open
           set current view of container window to icon view
           set toolbar visible of container window to false
           set statusbar visible of container window to false
           set the bounds of container window to {400, 100, 885, 420}
           set theViewOptions to the icon view options of container window
           set arrangement of theViewOptions to not arranged
           set icon size of theViewOptions to 72
           set background picture of theViewOptions to file ".background:'background.png'"
           make new alias file at container window to POSIX file "/Applications" with properties {name:"Applications"}
           set position of item "'${2}'" of container window to {100, 100}
           set position of item "Applications" of container window to {375, 100}
           update without registering applications
           delay 5
           close
     end tell
   end tell
' | osascript
chmod -Rf go-w /Volumes/"${2}"
sync
sync
hdiutil detach ${device}
hdiutil convert "./temp.dmg" -format UDZO -imagekey zlib-level=9 -o "./${2}.dmg"
rm -f ./temp.dmg
