Application Controller PS3 v1.11
==========

The purpose of this application is to:
	- Allow the PS3 to control the PC via PS3 controller(s)
	- Display the current screen or application window at high quality on the TV with little lag

Credits
-------

	Dnawrkshp		- Creator
	STLcardsWS		- Concept
	Hermes			- sprite2D Tiny3D sample
	Deroad			- NoRSX image resizer
	

Compiling
-------

In order to compile this project you must have PSL1GHT installed properly.
If you don't have PSL1GHT you may grab it from their website: http://psl1ght.com/ or their github www.github.com/HACKERCHANNEL/PSL1GHT.
	
Building
	cd ApplicationControllerPS3
	make

Running with PS3Load
	cd ApplicationControllerPS3
	make run
	
Making a package
	cd ApplicationControllerPS3
	make pkg
	
	
Status
-------

	+	Send PC screen to PS3
	-	Send specific window to PS3
	-	Send 100% quality image to PS3 at over 15 FPS
	+	Retrieve pad state for any controller
	+	Use pad states to interact with PC programs
	+	Have user entered pad input configuration

