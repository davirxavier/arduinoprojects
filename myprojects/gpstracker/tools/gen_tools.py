from base64 import b64encode
from Crypto.Protocol.KDF import HKDF
from Crypto.Hash import SHA256
from Crypto.Cipher import ChaCha20_Poly1305
from Crypto.Random import get_random_bytes
import secrets
import string

alphabet = string.ascii_letters + string.digits


def to_bytes(s):
    return bytes(s, 'utf-8')


def random_str(length):
    return ''.join(secrets.choice(alphabet) for i in range(length))


def gen_user_key(uuid, username, salt, key):
    return SHA256.new(to_bytes(uuid + username + key + salt)).digest().hex()


def encrypt_str(text: string, pwd: string):
    salt = get_random_bytes(32)
    hashedkey = HKDF(pwd.encode(), 32, salt, SHA256)

    nonce = get_random_bytes(12)

    cipher = ChaCha20_Poly1305.new(key=hashedkey, nonce=nonce)
    ciphertext, tag = cipher.encrypt_and_digest(text.encode())

    return {
        "ciphertext": b64encode(ciphertext).decode(),
        "salt": b64encode(salt).decode(),
        "nonce": b64encode(nonce).decode(),
        "tag": b64encode(tag).decode(),
    }


def decrypt_str(text: bytes, pwd: string, salt: bytes, nonce: bytes, tag: bytes):
    hashedkey = HKDF(pwd.encode(), 32, salt, SHA256)

    cipher = ChaCha20_Poly1305.new(key=hashedkey, nonce=nonce)
    plaintext = cipher.decrypt_and_verify(text, received_mac_tag=tag)

    return plaintext.decode()
