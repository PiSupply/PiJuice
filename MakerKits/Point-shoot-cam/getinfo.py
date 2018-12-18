from pijuice import PiJuice # Import pijuice module
pijuice = PiJuice(1, 0x14) # Instantiate PiJuice interface object
print pijuice.status.GetStatus() # Read PiJuice staus.
print pijuice.status.GetChargeLevel()
