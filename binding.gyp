{
    "targets": [
        {
            "target_name": "hotkeys",
            "sources": ["hotkeys.cc"],
            "libraries": ["psapi.lib"],
            "include_dirs": [
                "<!(node -e \"require('nan')\")"
            ]
        }
    ]
}