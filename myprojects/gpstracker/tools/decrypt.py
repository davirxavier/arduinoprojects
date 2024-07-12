import sys
from base64 import b64decode
from Crypto.Protocol.KDF import HKDF
from Crypto.Hash import SHA256
from Crypto.Cipher import ChaCha20_Poly1305
from Crypto.Util import Padding

if len(sys.argv) < 3:
    print('invalid parameters')
    exit(-1)

rawdata = sys.argv[1].strip()
keystr = sys.argv[2]

if not rawdata or not keystr:
    print('invalid parameters')
    exit(-1)

saltsize = int(rawdata[:3])
saltposend = 3+saltsize
salt = bytes.fromhex(rawdata[3:saltposend])
print("Salt: " + rawdata[3:saltposend])

noncesize = int(rawdata[saltposend:saltposend+2])
nonceposend = saltposend+2+noncesize
nonce = bytes.fromhex(rawdata[saltposend+2:nonceposend])
print("Nonce: " + rawdata[saltposend+2:nonceposend])

tagsize = int(rawdata[nonceposend:nonceposend+2])
tagposend = nonceposend+2+tagsize
tag = bytes.fromhex(rawdata[nonceposend+2:tagposend])
print("Tag: " + rawdata[nonceposend+2:tagposend])

print("Raw data:" + rawdata[tagposend:])
data = bytes.fromhex(rawdata[tagposend:])

key = Padding.pad(bytes(keystr, 'utf-8'), 32)
hashedkey = HKDF(key, 32, salt, SHA256)

print()

try:
    cipher = ChaCha20_Poly1305.new(key=hashedkey, nonce=nonce)
    plaintext = cipher.decrypt_and_verify(data, tag)
    print("Decrypted text: " + plaintext.decode())
except (ValueError, KeyError):
    print("Incorrect decryption")
