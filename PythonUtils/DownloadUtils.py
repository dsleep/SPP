import subprocess
import sys
import os
import requests
from awsauth import S3Auth

def get_script_path():
	return os.path.dirname(os.path.realpath(__file__))
	
def which(program):

	def is_exe(fpath):
		return os.path.isfile(fpath) and os.access(fpath, os.X_OK)

	fpath, fname = os.path.split(program)
	if fpath:
		if is_exe(program):
			return program
	else:
		for path in os.environ["PATH"].split(os.pathsep):
			exe_file = os.path.join(path, program)
			if is_exe(exe_file):
				return exe_file
	return None
	
def RunAndWait(ProgramLaunch):
	print("Running {}...".format(ProgramLaunch))
	process = subprocess.Popen(ProgramLaunch, bufsize=2048, shell=True, stdout=subprocess.PIPE, encoding='utf8', close_fds=True)
		
	while True:
		output = process.stdout.readline()
		if output == '' and process.poll() is not None:
			break
		if output:
			print(output, end = '')
	rc = process.poll()
	return "DONE"	

def Download(URL, file_name):
	with open(file_name, "wb") as f:
		print("Downloading %s" %(URL))
		response = requests.get(URL, stream=True)
		total_length = response.headers.get('content-length')

		if total_length is None: # no content length header
			f.write(response.content)
		else:
			dl = 0
			total_length = int(total_length)
			for data in response.iter_content(chunk_size=(1 * 1024 * 1024)):
				dl += len(data)
				f.write(data)
				done = int(50 * dl / total_length)
				sys.stdout.write("\r[%s%s]" % ('=' * done, ' ' * (50-done)) )    
				sys.stdout.flush()

def DownloadFromS3(URL, ACCESS_KEY, SECRET_KEY, file_name):

	with open(file_name, "wb") as f:
		print("Downloading %s" %(URL))
		response = requests.get(URL, auth=S3Auth(ACCESS_KEY, SECRET_KEY), stream=True)
		total_length = response.headers.get('content-length')

		if total_length is None: # no content length header
			f.write(response.content)
		else:
			dl = 0
			total_length = int(total_length)
			for data in response.iter_content(chunk_size=(1 * 1024 * 1024)):
				dl += len(data)
				f.write(data)
				done = int(50 * dl / total_length)
				sys.stdout.write("\r[%s%s]" % ('=' * done, ' ' * (50-done)) )    
				sys.stdout.flush()
				
def DownloadAndInstall(URL, file_name):
	Download(URL, file_name)				
	RunAndWait(file_name);
		