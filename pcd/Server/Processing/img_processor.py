### Script Python care face toată procesarea imaginilor pentru server ### 

# Pentru a testa scriptul în timpul întreținerii, utilizarea este după cum urmează...
# python3 img_processor.py [option] [calea imaginii de intrare] [calea imaginii de ieșire]

# Opțiunile posibile sunt:
# -none            Nici o transformare. Se creează doar o copie. 
# -grayscale       Pentru a converti imaginea de intrare în tonuri de gri.
# -invert          Pentru a inversa culorile unei imagini de intrare și a o salva ca o nouă imagine. 
# -rotate:x        Pentru a roti o imagine de intrare cu "x" grade în sens invers acelor de ceasornic. 
# -pyramid_up      Filtru Pyramid Up. 
# -pyramid_down    Filtru piramidă în jos. 
# -edgedetect      Pentru a genera o imagine de ieșire care conține marginile detectate ale unei imagini de intrare. 

# -blur            Pentru a estompa o imagine folosind Gaussian Blurring. 


# Notă: Opțiunea de rotire trebuie utilizată astfel:
# -rotate:x  ,unde x este unghiul în grade care urmează să fie rotit în sens invers acelor de ceasornic. 


# Mai întâi importăm biblioteca Python Imaging Library pentru a ne ajuta în sarcinile de bază. 

from PIL import Image, ImageFilter

# Apoi vom importa OpenCV pentru a ne ajuta să realizăm sarcini complicate de procesare a imaginilor.
import cv2

# Utilizăm modulul "sys" pentru analizarea argumentelor din linia de comandă.
import sys


# Funcție care implementează funcționalitatea "principală" a scriptului.
# Acesta va analiza argumentele din linia de comandă și va transforma imaginea de intrare în imaginea de ieșire cu modificările efectuate conform opțiunii.  
def main():
	# Numărul de argumente din linia de comandă trebuie să fie 4 (inclusiv numele scriptului).
	# În caz contrar, se tipărește un mesaj de eroare prin care se declară că nu există argumente suficiente,
	# și iese cu codul de eroare 1
	# Deoarece acest script este rulat de către executabilul serverului, această eroare nu ar trebui să apară. 
	
	if (len(sys.argv) < 4):
		sys.exit(1)
		
	# Acum verificăm opțiunile și efectuăm transformarea imaginilor de intrare, care vor fi salvate ca o nouă imagine în calea furnizată. 
	
	# Prindeți modul de transformare necesar. 
	option = sys.argv[1]
	
	if option == "-none":
		try:
			# Deschidem imaginea cu ajutorul PIL.
			img = Image.open(sys.argv[2])
			# creăm o copie a imaginii.
			
			img.save(sys.argv[2])
			
		except:
			sys.exit(8)
		
	
	elif option == "-grayscale":
		# Convertim imaginea dată în tonuri de gri. 
		try:
			# Folosind PIL deschidem direct imaginea.
			img = Image.open(sys.argv[2])
			
			# Convertim imaginea în tonuri de gri prin specificarea "L"  
			img = img.convert("L")
			
			# În cele din urmă îl salvăm pe disc, pe calea necesară. 
			img.save(sys.argv[3])
			
			
		except:
			# În cazul în care se aruncă o excepție 
			# ieșim cu un cod de eroare de 2 
			sys.exit(2)
			
	elif option == "-invert":
			# Am inversat imaginea folosind OpenCV
			try:
				img = cv2.imread(sys.argv[2])
				
				# Inversarea culorilor...
				img = cv2.bitwise_not(img)
				
				# Se salvează imaginea în calea necesară.
				cv2.imwrite(sys.argv[3], img)
			except:
				
				# Dacă apare vreo eroare, ieșim cu un cod de 3.
				
				sys.exit(3)
				
	elif option.find("-rotate:") != -1:
				try:
					# Mai întâi analizăm unghiul de rotire. 
					t = float(option.split(":")[1])
					
					# Deschidem imaginea. 
					img = Image.open(sys.argv[2])
					
					# Rotăm imaginea folosind PIL.
					img = img.rotate(angle = t, expand = True)
					
					# În cele din urmă, salvăm imaginea rotită pe calea necesară.
					img.save(sys.argv[3])
						
				except:
					
					# Dacă întâlnim o eroare, ieșim cu un cod de eroare de 4
					sys.exit(4)
				
	elif option == "-pyramid_up":
				try:
					# Deschidem imaginea. 
					img = cv2.imread(sys.argv[2])
					
					# Efectuăm supraeșantionarea piramidei.
					
					img = cv2.pyrUp(img)
					
					# În cele din urmă, salvăm imaginea crescută în piramidă în locația necesară cu numele de fișier dorit.
					cv2.imwrite(sys.argv[3], img)
					
					
					
					
					
					
				except:
					# Dacă întâlnim o eroare generată de o excepție, ieșim cu un cod de eroare de 5.
					sys.exit(5)
					
				
	elif option == "-pyramid_down":
				try:
					# Deschidem imaginea.  
					img = cv2.imread(sys.argv[2])
					# Se efectuează eșantionarea piramidală a imaginii și, în final, se salvează imaginea transformată într-un nou fișier, după cum este necesar. 
					
					img = cv2.pyrDown(img)
					cv2.imwrite(sys.argv[3], img)
				except:
					# Dacă întâlnim o eroare, ieșim cu un cod de eroare de 6
					sys.exit(6)
					
				
	elif option == "-edgedetect":
		try:
			# Mai întâi deschidem imaginea pentru detectarea marginilor.
			img = cv2.imread(sys.argv[2])
			
			# Utilizăm filtrul de detectare a marginilor Canny Edge pentru a detecta marginile. 
			img = cv2.Canny(img, 100, 200)
			
			# În cele din urmă, salvăm rezultatul.
			
			cv2.imwrite(sys.argv[3], img)
			
		except:
			# Dacă întâlnim o eroare, ieșim cu un cod de eroare de 7
			sys.exit(7)
			
			
	elif option == "-blur":
		try:
			
			# Mai întâi deschidem imaginea folosind PIL
			img = Image.open(sys.argv[2])
			
			# Se aplică filtrul Gaussian Blur
			img_blurred = img.filter(ImageFilter.GaussianBlur(radius = 10))
			
			# Salvăm imaginea neclară la destinația dorită.
			
			img_blurred.save(sys.argv[3])
			
			
			
		except:
			#  Dacă apare o eroare, se forțează ieșirea cu un cod de eroare de 9.
			sys.exit(9)
		
if __name__ == "__main__":
	main()


