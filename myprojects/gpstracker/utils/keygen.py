import sys

from Crypto.Hash import SHA256


def to_bytes(s):
    return bytes(s, 'utf-8')


print(SHA256.new(to_bytes(sys.argv[1] + sys.argv[2] + sys.argv[3] + sys.argv[4])).digest().hex())
