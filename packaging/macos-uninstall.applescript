-- Compiled into "Uninstall PurrOne.app" by the release workflow (osacompile). Installed to /Applications by the .pkg.
-- Removes everything the installer put on the Mac. Saved presets are kept unless you choose to delete them.
set productName to "Whisker Scope"
set presetsDir to (POSIX path of (path to application support folder from user domain)) & "Studio 87/" & productName
set oldPresetsDir to (POSIX path of (path to application support folder from user domain)) & "SpeakingColors/" & productName
display dialog "Remove " & productName & " from this Mac?" & return & return & "This deletes the plugin, the standalone app, the manual and this uninstaller." buttons {"Cancel", "Remove"} default button "Cancel" with icon caution
set keepShapes to true
tell application "System Events" to set hasPresets to (exists folder presetsDir) or (exists folder oldPresetsDir)
if hasPresets then
	set r to display dialog "Also delete your saved shapes?" & return & return & presetsDir buttons {"Keep shapes", "Delete shapes"} default button "Keep shapes" with icon caution
	if button returned of r is "Delete shapes" then set keepShapes to false
end if
set cmds to "rm -rf '/Library/Audio/Plug-Ins/VST3/" & productName & ".vst3' '/Applications/" & productName & ".app' '/Library/Application Support/Studio 87/" & productName & "' '/Library/Application Support/SpeakingColors/" & productName & "' '/Applications/Uninstall " & productName & ".app'; "
set cmds to cmds & "rmdir '/Library/Application Support/Studio 87' '/Library/Application Support/SpeakingColors' 2>/dev/null; for p in $(pkgutil --pkgs | grep '^io.github.speakingcolors.whiskerscope'); do pkgutil --forget $p; done"
do shell script cmds with administrator privileges
-- standalone app settings (audio device choice only); exact file name unverified, removed if present
do shell script "rm -rf ~/Library/Application\\ Support/" & productName & " ~/Library/Preferences/" & productName & ".settings 2>/dev/null; true"
if keepShapes is false then
	do shell script "rm -rf " & quoted form of presetsDir & " " & quoted form of oldPresetsDir
	do shell script "rmdir " & quoted form of (POSIX path of (path to application support folder from user domain) & "Studio 87") & " " & quoted form of (POSIX path of (path to application support folder from user domain) & "SpeakingColors") & " 2>/dev/null; true"
end if
set doneMsg to productName & " was removed."
if keepShapes then set doneMsg to doneMsg & return & "Your saved shapes were kept."
display dialog doneMsg buttons {"OK"} default button "OK"
