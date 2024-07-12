import base64
import json
import os
import sys

import uuid
import firebase_admin
from firebase_admin import credentials
from firebase_admin import db
from firebase_admin import auth
from gen_tools import encrypt_str, random_str, gen_user_key
from Crypto.Util import Padding

if len(sys.argv) < 3:
    print('Wrong command parameters. Correct format is: python script <USER_EMAIL> <PASSWORD>')
    exit(-1)

user_email = sys.argv[1]
password = sys.argv[2]

generated_password = random_str(16)
config_password = random_str(32)

cred = credentials.Certificate('serviceAccountKey.json')
db_url = os.environ['DB_URL']

firebase_admin.initialize_app(cred, {
    'databaseURL': db_url
})

user = auth.create_user(
    email=user_email,
    password=generated_password,
    email_verified=True,
    disabled=False)

print('Sucessfully created new user: {0}'.format(user.uid))

vehicleUid = str(uuid.uuid4())

userKey = gen_user_key(user.uid, user_email, random_str(32), config_password)
json_str = json.JSONEncoder().encode({
    "databaseUrl": db_url,
    "vehicleUid": vehicleUid,
    "userKey": userKey,
    "encryptionKey": base64.b64encode(Padding.pad(generated_password.encode(), 32)).decode()
})

encrypt_conf = encrypt_str(json_str, config_password)

encrypt_conf['configPassword'] = config_password
encrypt_conf['vehicleUid'] = vehicleUid
config_json = json.JSONEncoder().encode(encrypt_conf)

ref = db.reference('users/' + userKey + '/' + vehicleUid)
ref.set("")

f = open('config_info', 'w')
f.write(json.JSONEncoder().encode(encrypt_str(config_json, password)))
f.close()
