# Documentation

### File access

Files are stored with their SHA-1 value as their name, with the first two digits being the subfolder name. For each user files belonging to them a represented in a .json file, which is managed by a central process; on runtime contents are stored in memory for thread safety.

Potential issues:
- Accessing actual files
- Safing on exit