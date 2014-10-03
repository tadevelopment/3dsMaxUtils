3dsMaxUtils
===========

Helper utilities for developing with 3ds Max


This repository contains code for automating and/or safety-proofing some of the repetitive tasks when developing 3ds max plugins.

Undo/Redo
=========

Use the DataRestoreObj container to add generic data to max's undo/redo system.  It automatically handles fringe cases like a suspended undo system, repeat adds, etc.

References
==========

The RefMgr and RefPtr system implement 3ds max's reference system in a safe and consistent manner.  Instructions on how to use them are included in the comments in the header files.
