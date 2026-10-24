-- DMG layout setup script.
--
-- Invoked by CPack's DragNDrop generator with the mounted disk image name as
-- the first argument. Configures the Finder window the user sees when they
-- open the DMG: hides the toolbar/status bar, sets a tidy window size, and
-- places the app icon on the left with the Applications symlink on the right.
--
-- cmake/Packaging.cmake supplies the background through CPACK_DMG_BACKGROUND_IMAGE.

on run argv
    set image_name to item 1 of argv

    tell application "Finder"
        tell disk image_name
            open

            set current view of container window to icon view
            set toolbar visible of container window to false
            set statusbar visible of container window to false
            
            -- NATIVE CORRECTION: Adjust window height to 430 to accommodate the native title bar.
            -- Width: 1000 - 400 = 600px | Height: 530 - 100 = 430px (400px canvas + 30px native title bar)
            set the bounds of container window to {400, 100, 1000, 530}

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

            -- NATIVE CORRECTION: Vertically center the icons on the true 400px usable canvas height.
            -- X coordinates are perfectly mirrored at 150px inward from the left and right edges.
            set position of item "CoreDeck.app" of container window to {150, 230}
            set position of item "Applications" of container window to {450, 230}

            -- Force Finder to write the layout to .DS_Store before CPack snapshots it.
            close
            open
            update without registering applications
            delay 3
        end tell
    end tell
end run
