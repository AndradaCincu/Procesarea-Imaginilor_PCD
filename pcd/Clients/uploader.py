##### UPLOADER CLIENT #####
# Acest script Python reproduce un script care încarcă fișiere imagine,
# pentru procesare către serverul nostru de procesare a imaginilor.
# Așteaptă ca imaginea să fie procesată și, de asemenea, descarcă imaginea transformată. 

import socket
import sys
import os

def main():
	
	print("##### UPLOADER CLIENT #####")
	
	# Primim de la utilizator adresa IP și portul serverului de procesare a imaginilor. 
	ip = input("Enter server IP: ")
	port = int(input("Enter server port: "))
	
	
	print("\n[*]Attempting to connect to server at: {0} on port: {1}".format(ip, port))
	
	with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
		# Ne conectăm la serverul nostru. 
		try:
			sock.connect((ip, port))
			
			
		    # Îi spunem serverului că suntem clientul încărcător.
		    # Ne asigurăm că trimitem șiruri de caractere cu terminație nulă. 
				
			sock.send("UPLOADER".encode("ascii") + b"\x00")
				
				# Primim răspunsul de la server.
			reply = sock.recv(1024)
			if reply == b"OK\x00":
				
				print("\n[*]SUCCESS: Server accepted our connection! ")
					
					# Solicităm utilizatorului să furnizeze un fișier de imagine pentru a-l încărca pe server.  
				while True:
					filepath = input("[*]Please provide path to image file to upload, to our server: ")
						# Verificăm dacă fișierul există cu adevărat, pe dispozitivul clientului! 
					if os.path.exists(filepath):
						break
					else:
						print("\n[!]Please provide correct path! \n")
							
				filename = filepath.split("/")[-1]
					
				sock.send(filename.encode("ascii") + b"\x00")
					
					# Verificăm răspunsul serverului și, dacă acesta este OK, încărcăm datele întregului fișier. 
				reply = sock.recv(1024)
				if reply == b"OK\x00":
					
					# Acum încărcăm dimensiunea fișierului pe serverul nostru de procesare a imaginilor. 
					# Acest lucru se face pentru ca bucla internă read() din cadrul serverului să poată citi corect întregul fișier. 
					
					sz = os.stat(filepath).st_size
					
					sock.send(str(sz).encode("ascii")+b"\x00")
					
					# Așteptăm răspunsul de la server înainte de a continua. 
					reply = sock.recv(1024)
					
					if reply == b"OK\x00":
						with open(filepath, "rb") as fObj:
							while True:
								data = fObj.read(4096)
								if len(data) == 0:
									print("\n[*]Successfully done uploading the file!\n")
									break
								
								# Încărcăm datele pe care le citim din fișierul nostru pe serverul nostru.
								sock.send(data)
								
						# Acum așteptăm răspunsul de la server...
						# Dacă serverul trimite "SUCCES", atunci conversia imaginii este realizată cu succes! 
						# Putem descărca în siguranță imaginea transformată (cu filtru aplicat) în memoria locală a aplicației clientului nostru. 
						
						reply = sock.recv(1024)
						
						if reply == b"SUCCESS\x00":
							print("\n[*]The server successfully applied the transformation... \n")
						else:
							print("\n[!]ERROR: Server could NOT successfully do the transformation !\n   Closing the connection! Please try again later! ")
							sock.close()
							return (1)
						
							
						#~~~~~~~~~~~~~~~~~~
						# acă totul este BINE până acum, script-ul nostru ajunge aici!	 here!	
						# Acum trimitem mesajul "NEWSIZE" către server. Pentru a întreba despre noua dimensiune a fișierului transformat. 
						
						sock.send("NEWSIZE".encode("ascii")+b"\x00")
						
						
						# Primim dimensiunea noului fișier transformat. 
						
						# Deoarece este un șir de caractere, trebuie să îl convertim în int.
						
						size_str = sock.recv(1024)
						#  Îndepărtăm terminatorul nul C din șirul de octeți.
						size_str = size_str[0:len(size_str)-1]
						
						size = int(size_str)
						
						print("[*] The transformed file will have size of: {} bytes.".format(size))
						
						filepath = input("[*]Please provide file path with name to save (It should have same extension as the uploaded file.): ")
						
						# Noi trimitem comanda "TRANSFER" pentru a iniția transferul!  
						
						sock.send("TRANSFER".encode("ascii")+b"\x00")
						
						# Primim, în sfârșit, mult așteptata imagine transformată.
						
						with open(filepath, "wb") as download:
							n = 0 # Numărul de bytes primiți de la socket.
							while n < size:
								data = sock.recv(4096)
								download.write(data)
								n += len(data)
								
						
						
						print("\n[*]Transformed file downloaded to: '{}' \n".format(filepath))
						
						
						
					
			else:
				print("\n[!]ERROR: Server did not accept us as a supported client. ")
				
			
			# În sfârșit, închidem conexiunea cu serverul. 
			
			sock.close()
			
			print("\n[*] Thankyou very much for using the Uploader Client Application! :) ")
			
			
				
		except:
			print("\n[!]ERROR: Cannot connect to server! \n   Please try again! ")
			return 1
			
			
			
		
	
	
	
	
if __name__ == "__main__":
	main()
