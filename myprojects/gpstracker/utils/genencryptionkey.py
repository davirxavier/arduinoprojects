import sys
from base64 import b64encode
from Crypto.Util import Padding

s = bytes(sys.argv[1], 'utf-8')
padded = Padding.pad(s, 32)
print(padded.hex())
