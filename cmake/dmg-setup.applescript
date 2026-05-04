-- DMG layout setup script.
--
-- Invoked by CPack's DragNDrop generator with the mounted disk image name as
-- the first argument. Configures the Finder window the user sees when they
-- open the DMG: hides the toolbar/status bar, sets a tidy window size, and
-- places the app icon on the left with the Applications symlink on the right.
--
-- A background image can be added later by setting CPACK_DMG_BACKGROUND_IMAGE
-- in CMakeLists.txt and uncommenting the background line below.

on run argv
    set image_name to item 1 of argv

    tell application "Finder"
        tell disk image_name
            open

            set current view of container window to icon view
            set toolbar visible of container window to false
            set statusbar visible of container window to false
            set the bounds of container window to {400, 100, 1000, 500}

            set viewOptions to the icon view options of container window
            set arrangement of viewOptions to not arranged
            set icon size of viewOptions to 96
            set text size of viewOptions to 13
            -- CPack renames the background to .background/background.<ext> regardless
            -- of the source filename, so reference it by that fixed name.
            try
                set background picture of viewOptions to file ".background:background.tiff"
            on error
                try
                    set background picture of viewOptions to file ".background:background.png"
                end try
            end try

            set position of item "CoreDeck.app" of container window to {150, 185}
            set position of item "Applications" of container window to {450, 185}

            -- Force Finder to write the layout to .DS_Store before CPack snapshots it.
            close
            open
            update without registering applications
            delay 3
        end tell
    end tell

    -- Tell Finder this volume has a custom icon (kHasCustomIcon flag) so the
    -- staged .VolumeIcon.icns is used as the disk icon on the desktop, sidebar,
    -- and About-this-Mac. Without this flag macOS shows the default DMG icon.
    try
        do shell script "SetFile -a C " & quoted form of ("/Volumes/" & image_name)
    end try
end run