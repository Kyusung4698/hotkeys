{
    "targets": [
        {
            "target_name": "hotkeys",
            "sources": ["hotkeys.cc"],
            "include_dirs": [
                "<!(node -e \"require('nan')\")"
            ]
        }
    ]
}