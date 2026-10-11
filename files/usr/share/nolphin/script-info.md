#### Scripts

When executed from a local folder, scripts will be passed 
the selected file names. When executed from a remote folder
(e.g. a folder showing web or ftp content), scripts will
be passed no parameters.

In all cases, the following environment variables will be
set by Nolphin, which the scripts may use:

- NOLPHIN_SCRIPT_SELECTED_FILE_PATHS: newline-delimited paths for selected files (only if local)
- NOLPHIN_SCRIPT_SELECTED_URIS: newline-delimited URIs for selected files
- NOLPHIN_SCRIPT_CURRENT_URI: URI for current location
- NOLPHIN_SCRIPT_WINDOW_GEOMETRY: position and size of current window
- NOLPHIN_SCRIPT_NEXT_PANE_SELECTED_FILE_PATHS: newline-delimited paths for selected files in the inactive pane of a split-view window (only if local)
- NOLPHIN_SCRIPT_NEXT_PANE_SELECTED_URIS: newline-delimited URIs for selected files in the inactive pane of a split-view window
- NOLPHIN_SCRIPT_NEXT_PANE_CURRENT_URI: URI for current location in the inactive pane of a split-view window


