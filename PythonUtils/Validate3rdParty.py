#!/usr/bin/env python

import DownloadUtils

import py7zr
import os
import sys
import traceback
import json
import platform
		
def GetJSONName():
	SystemName = platform.system()
	MachineType = platform.machine()	
	
	if SystemName == 'Windows':
		return 'Prereqs_VS2019.json'
	elif SystemName == 'Linux':	
		if MachineType == 'aarch64':		
			return 'unknown'
		else:
			return 'unknown'
	else:
		return 'unknown'

def CreateArchives():

	SevenZipExec = '7z'

	SystemName = platform.system()
	if SystemName == 'Windows':
		print(DownloadUtils.RunAndWait("rmdir /s /q DISTRIBUTION"))
		SevenZipExec = os.environ.get('7ZIPEXEC')
		
	print("Building 3rd Party... " + SevenZipExec)	
	
	print(os.getcwd())
	os.chdir("./3rdParty")
	print(os.getcwd())
	
	JSONFileName = GetJSONName()
	with open(JSONFileName) as json_file:

		os.chdir("./binaries")

		data = json.load(json_file)	

		for curModule in data['modules']:
			print('Path: ' + curModule['Path'])
			print('Version: ' + str(curModule['Version']))
			print('')			
			
			RequiredVersion = curModule['Version']
			ActiveZip = "./DISTRIBUTION/{0}_V{1}.7z".format(curModule['Path'], RequiredVersion)
			
			print("ZIPPING WITH 7zip!!!")				
			
			print(DownloadUtils.RunAndWait('"{0}" a "{1}" {2}'.format(SevenZipExec, ActiveZip, curModule['Path']) ))

		os.chdir("../")
			
	os.chdir("../")
			
				
def CheckPrereqs():
	SystemName = platform.system()

	JSONFileName = GetJSONName()
	print("Checking 3rd Party..." + JSONFileName)
	
	os.chdir("./3rdParty")	
		
	with open(JSONFileName) as json_file:
		data = json.load(json_file)	
		
		try:  
			os.mkdir("DOWNLOADS") 
		except OSError as error:  
			print('making DOWNLOADS')

		urlRoot = data['urlroot'];

		for curModule in data['modules']:
			print('Path: ' + curModule['Path'])
			print('Version: ' + str(curModule['Version']))
			print('')
			
			OurVersion = 0
			RequiredVersion = int(curModule['Version'])

			TextFileName = './VERSION_{0}.txt'.format(curModule['Path'])
				
			try:
				with open(TextFileName) as f:
					Lines = f.readlines()
					if len(Lines) > 0:
						OurVersion = int(Lines[0])
						print("Our 3rd Party Version " + str(OurVersion))		
					
			except Exception:
				OurVersion = 0
				print("No Version " + TextFileName)
				
			if OurVersion != RequiredVersion:
			
				ActiveZip = "{0}_V{1}.7z".format(curModule['Path'], RequiredVersion)
				ActiveZipPath = "./DOWNLOADS/" + ActiveZip
				
				print("You have version %d, you need version %d " %(OurVersion,RequiredVersion))
								
				# Downloading a file
				DownloadUtils.Download(urlRoot + ActiveZip, ActiveZipPath)
									
				print("UNZIPPING WITH 7zip!!!")
				
				with py7zr.SevenZipFile(ActiveZipPath, mode='r') as z:
					z.extractall(path='./TEMP_3RDPARTY')

							
				if SystemName == 'Windows':
					print(DownloadUtils.RunAndWait("Robocopy ./TEMP_3RDPARTY/{0} ./{0} /MIR".format(curModule['Path'])))
					print(DownloadUtils.RunAndWait("rmdir /s /q TEMP_3RDPARTY"))
				else:
					print(DownloadUtils.RunAndWait("rsync -a ./TEMP_3RDPARTY/{0} ./".format(curModule['Path'])))
					print(DownloadUtils.RunAndWait("rm -r TEMP_3RDPARTY"))
								
					
				if "BinCopy" in curModule:
					for binCopy in curModule['BinCopy']:
					
						if SystemName == 'Windows':
					
							hasWildCard = binCopy.find('*') 
							
							if hasWildCard == -1:
								print(DownloadUtils.RunAndWait("Robocopy ./{0}/{1} {2} /NP /NFL /NDL /E".format( curModule['Path'], binCopy, "../Binaries")))
							else:
								copyPath = binCopy[:hasWildCard]
								wildcardValue = binCopy[hasWildCard:]
								print(DownloadUtils.RunAndWait("Robocopy ./{0}/{1} {2} /IF {3} /NP /NFL /NDL /E".format( curModule['Path'], copyPath, "../Binaries", wildcardValue)))						
						else:
							print(DownloadUtils.RunAndWait("cp ./{0}/{1} {2}".format(curModule['Path'], binCopy, "../Binaries")))
							
						
				with open(TextFileName, 'w') as f:
					f.write( str(RequiredVersion) )
					f.close()
					
				print("Setup Binary from 3rd Party...")	
				
		os.chdir("../")
				
	os.chdir("../")				

if __name__ == '__main__':
	CheckPrereqs()	
