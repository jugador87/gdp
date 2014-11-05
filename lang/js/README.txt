// JavaScript GDP access application programs and support libraries
// 2014-11-04
// Alec Dara-Abrams


// ===================================================================
Makefile

Recursive make for gdpjs/ and apps/.  Currently, apps/ does not build
anything.

// ===================================================================
README.txt
This file.


// ===================================================================
apps/

JS standalone applications programs.  Should provide examples of access
to GDP from Node.js JavaScript.  Run with Node.js .  See apps/README.txt .


// ===================================================================
gdpjs/

JS and C support libraries.  Has a local Makefile.


// ===================================================================
libs/

Will hold shared and dynamic libraries built down in gdpjs/


// ===================================================================
node_modules/

Node.js modules required by these JS programs and libraries.
Loaded via "npm install <module_name>" .


// ===================================================================
