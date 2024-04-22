with open("/dev/sculldev", "r") as character:
   character.seek(1)
   print(character.read(2))
   character.seek(0)
   print(character.read(2))