# Script to add x509 certificate for public key certificate creation
# The certificate used will be ROM format
#
# Python 3.10 script

import sys
import bin2c
import argparse
import os
import subprocess
import binascii
from re import sub, search
from random import randint
import shutil
import json
from textwrap import dedent
from hkdf import hkdf

g_sha_to_use = "sha512"

# Dictionaries used to store information about various extensions

hash_algo: dict[str, int] = {
    "SHA384": 2,
    "SHA512": 4,
    "SHA256": 6
}

private_key_type: dict[str, int] = {
    "RSA4K"     : 0,
    "BRP512"    : 1,
    "SECP256"   : 2,
    "SECP384"   : 3,
    "SECP521"   : 4,
}

###########################################################################################################

g_sha_oids = {
    "sha256": "2.16.840.1.101.3.4.2.1",
    "sha384": "2.16.840.1.101.3.4.2.2",
    "sha512": "2.16.840.1.101.3.4.2.3",
    "sha224": "2.16.840.1.101.3.4.2.4",
}

decryption_mode: dict[str, int] = {
    "ECB": 0,
    "CBC": 1,
    "CTR": 2,
    "CFB": 8,
}

g_x509_template = '''
[ req ]
distinguished_name         = req_distinguished_name
x509_extensions            = v3_ca
prompt                             = no

dirstring_type = nobmp

[ req_distinguished_name ]
C                                          = US
ST                                         = SC
L                                          = Dallas
O                                          = Texas Instruments., Inc.
OU                                         = PBU
CN                                         = Albert
emailAddress               = Albert@ti.com

[ v3_ca ]
basicConstraints = CA:true
1.3.6.1.4.1.294.1.1=ASN1:SEQUENCE:boot_seq
1.3.6.1.4.1.294.1.9=ASN1:SEQUENCE:keyring_ext
{KEYRING_ASYMM}
{KEYRING_SYMM}
{KEYRING_PRIVATE_ASYMM}
{KEYRING_CUSTOM_DATA}
1.3.6.1.4.1.294.1.16=ASN1:SEQUENCE:keyring_index

[ boot_seq ]
certType         =      INTEGER:{CERT_TYPE}
bootCore         =      INTEGER:0
bootCoreOpts     =      INTEGER:0
destAddr         =      FORMAT:HEX,OCT:00000000
imageSize        =      INTEGER:0

[ keyring_ext ]
keyring_sw_rev=INTEGER:{KEYRING_SW_REV}
keyring_ver=INTEGER:{KEYRING_VER}
num_of_asymm_keys=INTEGER:{NUM_ASYMM}
num_of_symm_keys=INTEGER:{NUM_SYMM}
num_of_asymm_private_keys=INTEGER:{NUM_PRIVATE_ASYMM}
custom_key_data_available=INTEGER:{CUSTOM_KEY_DATA_PRESENT}

[ keyring_index ]
sign_key_id = INTEGER:{KEY_ID}
rsvd        = INTEGER:0
'''

keyring_ext_asymm_seq = '''
[ keyring_asymm ]
'''

keyring_ext_symm_seq = '''
[ keyring_symm ]
key_blob=FORMAT:HEX,OCT:{SYMM_KEY_BLOB}
inital_vector =  FORMAT:HEX,OCT:{SYMM_KEY_BLOB_ENC_IV}
random_string =  FORMAT:HEX,OCT:{SYMM_KEY_BLOB_ENC_RS}
enc_key_salt  =  FORMAT:HEX,OCT:{ROOT_KEY_DERIVE_SALT}
enc_key_id    =  INTEGER:{ROOT_KEY_ID}
decryption_mode =  INTEGER:{SYMM_KEY_BLOB_DECRYPTION_MODE}
'''

keyring_ext_private_asymm_seq = '''
[ keyring_private_asymm ]
key_blob=FORMAT:HEX,OCT:{ASYMM_PRIVATE_KEY_BLOB}
inital_vector =  FORMAT:HEX,OCT:{ASYMM_PRIVATE_KEY_BLOB_ENC_IV}
random_string =  FORMAT:HEX,OCT:{ASYMM_PRIVATE_KEY_BLOB_ENC_RS}
enc_key_salt  =  FORMAT:HEX,OCT:{ROOT_KEY_DERIVE_SALT}
enc_key_id    =  INTEGER:{ROOT_KEY_ID}
decryption_mode =  INTEGER:{ASYMM_PRIVATE_KEY_BLOB_DECRYPTION_MODE}
'''

keyring_custom_data_seq = '''
[ keyring_custom_data ]
key_blob=FORMAT:HEX,OCT:{KEYRING_CUSTOM_DATA_BLOB}
inital_vector =  FORMAT:HEX,OCT:{KEYRING_CUSTOM_DATA_BLOB_ENC_IV}
random_string =  FORMAT:HEX,OCT:{KEYRING_CUSTOM_DATA_BLOB_ENC_RS}
enc_key_salt  =  FORMAT:HEX,OCT:{ROOT_KEY_DERIVE_SALT}
enc_key_id    =  INTEGER:{ROOT_KEY_ID}
decryption_mode =  INTEGER:{KEYRING_CUSTOM_DATA_BLOB_DECRYPTION_MODE}
offfset =  INTEGER:{KEYRING_CUSTOM_DATA_BLOB_OFFSET}
blob_length =  INTEGER:{KEYRING_CUSTOM_DATA_BLOB_LENGTH}
'''


def pub_pem_to_pub_der(input: str, output: str) -> None:
    """Converts a public key from PEM format to DER format.

    Args:
        input (str): The path to the public key file in PEM format.
        output (str): The path to the output file in DER format.
    """

    subprocess.check_output(
        ' openssl pkey -in {} -pubin -outform der -out {}'.format(input, output), shell=True)


def priv_pem_to_priv_der(input: str, output: str) -> None:
    """Converts a private key from PEM format to DER format.

    Args:
        input (str): The path to the private key file in PEM format.
        output (str): The path to the output file in DER format.
    """

    subprocess.check_output(
        'openssl pkey -in {} -outform der -out {}'.format(input, output), shell=True)


def calc_hash(input: str, output: str, hash_algo: str) -> None:
    """Calculates hash of public key file in DER format.

    Args:
        input (str) : The path to the public key file in DER format.
        output (str): The path to the output file containing public key hash.
        hash_algo (str): Algorithm used for hash calculation.
    """
    if (hash_algo == "SHA256"):
        subprocess.check_output(
            'openssl dgst -sha256 -binary {} > {}'.format(input, output), shell=True)
    elif (hash_algo == "SHA512"):
        subprocess.check_output(
            'openssl dgst -sha512 -binary {} > {}'.format(input, output), shell=True)
    elif (hash_algo == "SHA384"):
        subprocess.check_output(
            'openssl dgst -sha384 -binary {} > {}'.format(input, output), shell=True)
    else:
        print("Invalid hash algorithm")


def utils_hex_from_file(filename: str, print_to_stdout=False, out_file=None, width=32) -> str:
    """Converts the binary data in a file to hexadecimal format.

Args:
    filename (str): The name of the file to be read.
    print_to_stdout (bool): If True, prints the hex data to stdout.
    out_file (str): The name of the file to write the hex data to.
    width (int): The number of bytes to be printed per line.

Returns:
        str: The hex data as a string.
    """
    if not (os.path.exists(filename) and os.path.isfile(filename)):
        print(f"{filename} does not exist")
        return "ERROR READING FILE"

    hexdata = ""

    with open(filename, 'rb') as f:
        enckey = f.read()
        hexdata = binascii.hexlify(enckey).decode('ascii')

    line_list: list[str] = []
    if out_file or print_to_stdout:
        for i in range(0, len(hexdata), width):
            line_list.append(hexdata[i: i+width])

    if out_file:
        with open(str(out_file), 'w') as f:
            f.write('\n'.join(line_list))
            f.write('\n')

    if print_to_stdout:
        print('\n'.join(line_list))

    return hexdata


def populate_keyring_ext_symm(keys_data: dict, enc_key: str, aes_decryption_mode: str) -> None:
    """Populates symmetric keyring extension.

Args:
    keys_data (dict): Dict with keyring data

Returns:
    None
"""
    symm_keys = ""

    if ((enc_key is None) or (not os.path.exists(enc_key))):
        # Error, enc key has to be given
        print("Please give the key to be used for encryption of symmetric keys. It's either missing or file not found!")
        exit(1)
    else:
        enckey = None
        with open(enc_key, "rb") as f:
            enckey = f.read()
            if (args.kd_salt is not None):
                isalt = get_key_derivation_salt(args.kd_salt)
                isalt = bytearray(binascii.unhexlify(isalt))
                d_key = hkdf(32, enckey, isalt)
                enckey = binascii.hexlify(d_key).decode('utf-8')
            else:
                enckey = binascii.hexlify(enckey).decode('ascii')

    # we need the value of enc_iv as hex, so convert the bytes output to hex
    enc_iv = subprocess.check_output('openssl rand 16', shell=True)
    v_KEYRING_SYM_ENC_IV = binascii.hexlify(enc_iv).decode('ascii')

    # we don't need the value of enc_rs as hex for encryption, so keep the bytes object
    enc_rs = subprocess.check_output('openssl rand 32', shell=True)
    v_KEYRING_SYM_ENC_RS = binascii.hexlify(enc_rs).decode('ascii')

    for iter in range(keys_data["num_of_symm_keys"]):
        temp_keys = ''

        # Print individual symmetric key attributes
        print(f"\n[ symm_key{iter} ]")
        key_id = int(keys_data['keyring_symm'][iter]['key_id'])
        print(f"key_id=INTEGER:{key_id}")

        key_rights = keys_data['keyring_symm'][iter]['key_rights']
        print(f"key_rights=FORMAT:HEX,OCT:{key_rights}")

        key_length_bits = keys_data['keyring_symm'][iter]['key_length']
        key_length_bytes = int(key_length_bits/8)
        print(f"key_length={key_length_bits} bits ({key_length_bytes} bytes)")

        aes_key_hex = utils_hex_from_file(keys_data['keyring_symm'][iter]['aes_key']).ljust(64, '0')
        print(f"aes_key=FORMAT:HEX,OCT:{aes_key_hex}")

        # Populate index of auxiliary symmetric key from JSON data
        temp_keys += key_id.to_bytes(4, byteorder='little').hex()
        # Populate key rights of auxiliary symmetric key
        # Change key rights to little endian (00 00 00 0A) -> (0A 00 00 00)
        temp_keys += "".join(map(str.__add__,
                             key_rights[-2::-2], key_rights[-1::-2]))
        # Append key length in bytes
        temp_keys += key_length_bytes.to_bytes(4, byteorder='little').hex()
        # Append AES KEY
        # Fill the key with 0s for key sizes 16B and 24B
        temp_keys += aes_key_hex
        # Append the key blob with the current key in iteration
        symm_keys += temp_keys
    symm_keys += v_KEYRING_SYM_ENC_RS
    # create temp directory for temp files
    try:
        os.mkdir('tmpdir')
    except:
        None

    temp_key_blob = "key_blob"+str(randint(111, 999))

    # Pad zeros to a temporary binary to make the size multiple of 16
    zeros_pad = bytearray(16 - len(binascii.unhexlify(symm_keys)) % 16)

    with open('tmpdir/'+temp_key_blob, "ab") as f:
        # f.write(zeros_pad)
        f.write(bytearray(binascii.unhexlify(symm_keys)))
        f.write(zeros_pad)

    # # Finally generate the encrypted image
    subprocess.check_output(' openssl aes-256-{} -e -nopad -K {} -iv {}  -in {} -out {}'.format(aes_decryption_mode,
                                                                                                enckey, v_KEYRING_SYM_ENC_IV, 'tmpdir/'+temp_key_blob, 'tmpdir/'+temp_key_blob+'-enc'), shell=True)

    with open('tmpdir/'+temp_key_blob+'-enc', "rb") as f:
        symm_keys = f.read()

    return binascii.hexlify(symm_keys).decode('ascii'), v_KEYRING_SYM_ENC_IV, v_KEYRING_SYM_ENC_RS


def populate_keyring_ext_private_asymm(keys_data: dict, enc_key: str, aes_decryption_mode: str) -> None:
    """Populates private asymmetric keyring extension.

Args:
    keys_data (dict): Dict with keyring data

Returns:
    None
"""
    asymm_private_keys = ""

    if ((enc_key is None) or (not os.path.exists(enc_key))):
        # Error, enc key has to be given
        print("Please give the key to be used for encryption of private asymmetric keys. It's either missing or file not found!")
        exit(1)
    else:
        enckey = None
        with open(enc_key, "rb") as f:
            enckey = f.read()
            if (args.kd_salt is not None):
                isalt = get_key_derivation_salt(args.kd_salt)
                isalt = bytearray(binascii.unhexlify(isalt))
                d_key = hkdf(32, enckey, isalt)
                enckey = binascii.hexlify(d_key).decode('utf-8')
            else:
                enckey = binascii.hexlify(enckey).decode('ascii')

    # we need the value of enc_iv as hex, so convert the bytes output to hex
    enc_iv = subprocess.check_output('openssl rand 16', shell=True)
    v_KEYRING_PRIV_ASYM_ENC_IV = binascii.hexlify(enc_iv).decode('ascii')

    # we don't need the value of enc_rs as hex for encryption, so keep the bytes object
    enc_rs = subprocess.check_output('openssl rand 32', shell=True)
    v_KEYRING_PRIV_ASYM_ENC_RS = binascii.hexlify(enc_rs).decode('ascii')

    for iter in range(keys_data["num_of_private_asymm_keys"]):
        temp_keys = ''

        # Print individual symmetric key attributes
        print(f"\n[ asymm_private_key{iter} ]")
        key_id = int(keys_data['keyring_private_asymm'][iter]['key_id'])
        print(f"key_id=INTEGER:{key_id}")

        key_rights = keys_data['keyring_private_asymm'][iter]['key_rights']
        print(f"key_rights=FORMAT:HEX,OCT:{key_rights}")

        key_type_str = keys_data['keyring_private_asymm'][iter]['key_type']
        key_type = private_key_type[key_type_str]
        print(f"key_type=FORMAT:HEX,OCT:{key_type}")

        priv_key_der_path = os.path.join('tmpdir', f'priv_key_{iter}.der')
        priv_pem_to_priv_der(keys_data['keyring_private_asymm'][iter]['private_key'], priv_key_der_path)

        key_length_bytes = os.path.getsize(priv_key_der_path)
        print(f"key_length={key_length_bytes} bytes")
        asymm_private_key = utils_hex_from_file(priv_key_der_path)
        print(f"asymm_private_key=FORMAT:HEX,OCT:{asymm_private_key}")

        # Populate index of auxiliary symmetric key from JSON data
        temp_keys += key_id.to_bytes(4, byteorder='little').hex()
        # Populate key rights of auxiliary asymmetric private key
        # Change key rights to little endian (00 00 00 0A) -> (0A 00 00 00)
        temp_keys += "".join(map(str.__add__,
                             key_rights[-2::-2], key_rights[-1::-2]))
        # Append key type in bytes
        temp_keys += key_type.to_bytes(4, byteorder='little').hex()
        # Append key length in bytes
        temp_keys += key_length_bytes.to_bytes(4, byteorder='little').hex()
        # Append AES KEY
        # Fill the key with 0s for key sizes 16B and 24B
        temp_keys += asymm_private_key.ljust(5000, '0')
        # Append the key blob with the current key in iteration
        asymm_private_keys += temp_keys
    asymm_private_keys += v_KEYRING_PRIV_ASYM_ENC_RS
    # create temp directory for temp files
    try:
        os.mkdir('tmpdir')
    except:
        None

    temp_key_blob = "key_blob"+str(randint(111, 999))

    # Pad zeros to a temporary binary to make the size multiple of 16
    zeros_pad = bytearray(16 - len(binascii.unhexlify(asymm_private_keys)) % 16)

    with open('tmpdir/'+temp_key_blob, "ab") as f:
        # f.write(zeros_pad)
        f.write(bytearray(binascii.unhexlify(asymm_private_keys)))
        f.write(zeros_pad)

    # # Finally generate the encrypted image
    subprocess.check_output(' openssl aes-256-{} -e -nopad -K {} -iv {}  -in {} -out {}'.format(aes_decryption_mode,
                                                                                                enckey, v_KEYRING_PRIV_ASYM_ENC_IV, 'tmpdir/'+temp_key_blob, 'tmpdir/'+temp_key_blob+'-enc'), shell=True)

    with open('tmpdir/'+temp_key_blob+'-enc', "rb") as f:
        asymm_private_keys = f.read()

    return binascii.hexlify(asymm_private_keys).decode('ascii'), v_KEYRING_PRIV_ASYM_ENC_IV, v_KEYRING_PRIV_ASYM_ENC_RS

def populate_keyring_ext_custom_data(keys_data: dict, enc_key: str, aes_decryption_mode: str) -> None:
    """Populates custom data keyring extension.

Args:
    keys_data (dict): Dict with keyring data

Returns:
    tuple: (encrypted_blob_hex, enc_iv_hex, enc_rs_hex, offset, custom_data_length_bytes)
"""
    keyring_custom_data = ""

    if ((enc_key is None) or (not os.path.exists(enc_key))):
        # Error, enc key has to be given
        print("Please give the key to be used for encryption of private asymmetric keys. It's either missing or file not found!")
        exit(1)
    else:
        enckey = None
        with open(enc_key, "rb") as f:
            enckey = f.read()
            if (args.kd_salt is not None):
                isalt = get_key_derivation_salt(args.kd_salt)
                isalt = bytearray(binascii.unhexlify(isalt))
                d_key = hkdf(32, enckey, isalt)
                enckey = binascii.hexlify(d_key).decode('utf-8')
            else:
                enckey = binascii.hexlify(enckey).decode('ascii')

    # we need the value of enc_iv as hex, so convert the bytes output to hex
    enc_iv = subprocess.check_output('openssl rand 16', shell=True)
    v_KEYRING_CUSTOM_DATA_ENC_IV = binascii.hexlify(enc_iv).decode('ascii')

    # we don't need the value of enc_rs as hex for encryption, so keep the bytes object
    enc_rs = subprocess.check_output('openssl rand 32', shell=True)
    v_KEYRING_CUSTOM_DATA_ENC_RS = binascii.hexlify(enc_rs).decode('ascii')

    temp_data = ''

    # Print individual symmetric key attributes
    print(f"\n[ keyring_custom_data ]")
    offset = int(keys_data['keyring_custom_data'][0]['offset'])
    print(f"offset=INTEGER:{offset}")

    custom_data_file = keys_data['keyring_custom_data'][0]['custom_data']
    if not (os.path.exists(custom_data_file) and os.path.isfile(custom_data_file)):
        print(f"{custom_data_file} does not exist")
        exit(1)
    with open(custom_data_file, 'rb') as f:
        # Store the raw binary contents of the file in hexadecimal format
        custom_data = binascii.hexlify(f.read()).decode('ascii')
    print(f"custom_data=FORMAT:HEX,OCT:{custom_data}")

    custom_data_length_bytes = len(binascii.unhexlify(custom_data))
    print(f"custom_data_length={custom_data_length_bytes} bytes")

    # Append custom data bytes
    temp_data += custom_data

    # Append the key blob with the current key in iteration
    keyring_custom_data += temp_data
    keyring_custom_data += v_KEYRING_CUSTOM_DATA_ENC_RS

    # create temp directory for temp files
    try:
        os.mkdir('tmpdir')
    except:
        None

    temp_key_blob = "key_blob"+str(randint(111, 999))

    # Pad zeros to a temporary binary to make the size multiple of 16
    zeros_pad = bytearray(16 - len(binascii.unhexlify(keyring_custom_data)) % 16)

    with open('tmpdir/'+temp_key_blob, "ab") as f:
        # f.write(zeros_pad)
        f.write(bytearray(binascii.unhexlify(keyring_custom_data)))
        f.write(zeros_pad)

    # # Finally generate the encrypted image
    subprocess.check_output(' openssl aes-256-{} -e -nopad -K {} -iv {}  -in {} -out {}'.format(aes_decryption_mode,
                                                                                                enckey, v_KEYRING_CUSTOM_DATA_ENC_IV, 'tmpdir/'+temp_key_blob, 'tmpdir/'+temp_key_blob+'-enc'), shell=True)

    with open('tmpdir/'+temp_key_blob+'-enc', "rb") as f:
        keyring_custom_data = f.read()

    return binascii.hexlify(keyring_custom_data).decode('ascii'), v_KEYRING_CUSTOM_DATA_ENC_IV, v_KEYRING_CUSTOM_DATA_ENC_RS, offset, custom_data_length_bytes

def populate_keyring_ext_asymm(keys_data: dict) -> None:
    """Populates Asymmetric keyring extension.

    Args:
    keys_data (dict): Dict with keyring data

Returns:
    None
"""

    global keyring_ext_asymm_seq

    temp_string = "asymm_key1=SEQUENCE:comp1"
    asymm_keys = ''''''

    temp_comp = '''
[ comp1 ]
keyId=INTEGER:key_id_val
key_rights=FORMAT:HEX,OCT:key_rights_val
hash_algo=INTEGER:hash_algo_val
public_key=FORMAT:HEX,OCT:public_key_val
'''

    for iter in range(keys_data["num_of_asymm_keys"]):
        keyring_ext_asymm_seq += (temp_string.replace("1", str(iter)) + "\n")
        pub_pem_to_pub_der(keys_data['keyring_asymm'][iter]['pub_key'], os.path.join(
            'tmpdir', 'pub_key.der'))

        # calc_hash <INPUT> <OUTPUT>
        calc_hash(os.path.join('tmpdir', 'pub_key.der'),
                  os.path.join('tmpdir', 'pub_key_hash'),
                  keys_data['keyring_asymm'][iter]['hash_algo'])

        asymm_keys += temp_comp.replace('1', str(iter))\
            .replace('key_id_val', str(keys_data['keyring_asymm'][iter]['key_id']))\
            .replace('key_rights_val', keys_data['keyring_asymm'][iter]['key_rights'])\
            .replace('public_key_val', utils_hex_from_file(os.path.join('tmpdir', 'pub_key_hash')))\
            .replace('hash_algo_val', str(hash_algo[keys_data['keyring_asymm'][iter]['hash_algo']]))

    keyring_ext_asymm_seq += asymm_keys
    print(keyring_ext_asymm_seq)


def get_key_derivation_salt(kd_salt_file_name: str) -> str:
    kd_salt = None
    if (not os.path.exists(kd_salt_file_name)):
        # Error, key derivation salt has to be given
        print("Please give the key derivation salt file name. It's either missing or file not found!")
        exit(1)
    else:
        with open(kd_salt_file_name, "r") as f:
            kd_salt = f.read()
            kd_salt = kd_salt.strip('\n')

    return kd_salt


def get_cert(args) -> None:
    """
    This function reads keys from a JSON file and generates a certificate
    based on the keyring information.

    Args:
    args (argparse.Namespace)
    """
    # read keys from keys.json file
    with open(args.keys_info, 'r') as keys:
        keys_data = json.load(keys, strict=False)

    aes_decryption_mode = args.decrypt_mode
    sign_key_id = args.sign_key_id
    enc_key_id = args.enc_key_id

    # this is cert type unique to keyring certificate, this is required for TIFS to verify this certificate against the active root keys
    device_cert_type = 0xA5000002

    # Salt for derivation of root key, derived root key will be used for key blob decryption
    v_ROOT_KEY_DERIVE_SALT = '0000'

    # populate keyring_version from the dict
    if (keys_data["keyring_ver"] is None):
        # Default to 1
        keyring_ver = 1
    else:
        keyring_ver = keys_data["keyring_ver"]

    print('keyring version = ' + str(keyring_ver))
    print('keyring software revision = ' + str(keys_data["keyring_sw_rev"]))
    print('Number of asymmetric keys = ' + str(keys_data["num_of_asymm_keys"]))
    print('Number of symmetric keys = ' + str(keys_data["num_of_symm_keys"]))
    print('Number of private asymmetric keys = ' + str(keys_data["num_of_private_asymm_keys"]))
    print('Custom Key Data available = ' + str(keys_data["keyring_custom_data_available"]))

    # populate number of asymmetric keys from the dict
    if (keys_data["num_of_asymm_keys"] == 0):
        # Default to 0
        num_of_asymm_keys = 0
        keyring_asymm = ""
    else:
        num_of_asymm_keys = keys_data["num_of_asymm_keys"]
        keyring_asymm = "1.3.6.1.4.1.294.1.10=ASN1:SEQUENCE:keyring_asymm"
        populate_keyring_ext_asymm(keys_data)
    if (keys_data["num_of_symm_keys"] == 0):
        # Default to 0
        num_of_symm_keys = 0
        keyring_symm = ""
    else:
        v_ROOT_KEY_ID = enc_key_id
        num_of_symm_keys = keys_data["num_of_symm_keys"]
        keyring_symm = "1.3.6.1.4.1.294.1.11=ASN1:SEQUENCE:keyring_symm"
        symm_keys, v_KEYRING_SYM_ENC_IV, v_KEYRING_SYM_ENC_RS = populate_keyring_ext_symm(
            keys_data, args.enckey, aes_decryption_mode)
        if args.kd_salt:
            v_ROOT_KEY_DERIVE_SALT = get_key_derivation_salt(
                args.kd_salt)
            
    if (keys_data["num_of_private_asymm_keys"] == 0):
        # Default to 0
        num_of_private_asymm_keys = 0
        keyring_private_asymm = ""
    else:
        v_ROOT_KEY_ID = enc_key_id
        num_of_private_asymm_keys = keys_data["num_of_private_asymm_keys"]
        keyring_private_asymm = "1.3.6.1.4.1.294.1.17=ASN1:SEQUENCE:keyring_private_asymm"
        asymm_private_keys, v_KEYRING_PRIV_ASYM_ENC_IV, v_KEYRING_PRIV_ASYM_ENC_RS = populate_keyring_ext_private_asymm(
            keys_data, args.enckey, aes_decryption_mode)
        if args.kd_salt:
            v_ROOT_KEY_DERIVE_SALT = get_key_derivation_salt(
                args.kd_salt)
    
    if (keys_data["keyring_custom_data_available"] == "yes"):
        custom_key_data_avaliable = 1
        v_ROOT_KEY_ID = enc_key_id
        keyring_custom_data = "1.3.6.1.4.1.294.1.18=ASN1:SEQUENCE:keyring_custom_data"
        custom_data, v_KEYRING_CUSTOM_DATA_ENC_IV, v_KEYRING_CUSTOM_DATA_ENC_RS, v_KEYRING_CUSTOM_DATA_OFFSET, v_KEYRING_CUSTOM_DATA_LENGTH = populate_keyring_ext_custom_data(
            keys_data, args.enckey, aes_decryption_mode)
        if args.kd_salt:
            v_ROOT_KEY_DERIVE_SALT = get_key_derivation_salt(
                args.kd_salt)
    else:
        # Default to 0
        custom_key_data_avaliable = 0
        keyring_custom_data = ""

    ret_cert = g_x509_template.format(
        CERT_TYPE=device_cert_type,
        KEYRING_SW_REV=keys_data["keyring_sw_rev"],
        KEYRING_VER=keyring_ver,
        KEYRING_ASYMM=keyring_asymm,
        NUM_ASYMM=num_of_asymm_keys,
        KEYRING_SYMM=keyring_symm,
        NUM_SYMM=num_of_symm_keys,
        KEYRING_PRIVATE_ASYMM=keyring_private_asymm,
        NUM_PRIVATE_ASYMM=num_of_private_asymm_keys,
        KEYRING_CUSTOM_DATA=keyring_custom_data,
        CUSTOM_KEY_DATA_PRESENT=custom_key_data_avaliable,
        KEY_ID=sign_key_id
    )
    if (keys_data["num_of_asymm_keys"] > 0):
        ret_cert += keyring_ext_asymm_seq
    if (keys_data["num_of_symm_keys"] > 0):
        ret_cert += keyring_ext_symm_seq.format(
            SYMM_KEY_BLOB=symm_keys,
            SYMM_KEY_BLOB_ENC_IV=v_KEYRING_SYM_ENC_IV,
            SYMM_KEY_BLOB_ENC_RS=v_KEYRING_SYM_ENC_RS,
            ROOT_KEY_DERIVE_SALT=v_ROOT_KEY_DERIVE_SALT,
            ROOT_KEY_ID=v_ROOT_KEY_ID,
            SYMM_KEY_BLOB_DECRYPTION_MODE=decryption_mode[aes_decryption_mode],
        )
        print(ret_cert.split("[ keyring_symm ]", 1)[1])
    if (keys_data["num_of_private_asymm_keys"] > 0):
        ret_cert += keyring_ext_private_asymm_seq.format(
            ASYMM_PRIVATE_KEY_BLOB=asymm_private_keys,
            ASYMM_PRIVATE_KEY_BLOB_ENC_IV=v_KEYRING_PRIV_ASYM_ENC_IV,
            ASYMM_PRIVATE_KEY_BLOB_ENC_RS=v_KEYRING_PRIV_ASYM_ENC_RS,
            ROOT_KEY_DERIVE_SALT=v_ROOT_KEY_DERIVE_SALT,
            ROOT_KEY_ID=v_ROOT_KEY_ID,
            ASYMM_PRIVATE_KEY_BLOB_DECRYPTION_MODE=decryption_mode[aes_decryption_mode],
        )
        print(ret_cert.split("[ keyring_private_asymm ]", 1)[1])
    if (keys_data["keyring_custom_data_available"] == "yes"):
        ret_cert += keyring_custom_data_seq.format(
            KEYRING_CUSTOM_DATA_BLOB=custom_data,
            KEYRING_CUSTOM_DATA_BLOB_ENC_IV=v_KEYRING_CUSTOM_DATA_ENC_IV,
            KEYRING_CUSTOM_DATA_BLOB_ENC_RS=v_KEYRING_CUSTOM_DATA_ENC_RS,
            ROOT_KEY_DERIVE_SALT=v_ROOT_KEY_DERIVE_SALT,
            ROOT_KEY_ID=v_ROOT_KEY_ID,
            KEYRING_CUSTOM_DATA_BLOB_DECRYPTION_MODE=decryption_mode[aes_decryption_mode],
            KEYRING_CUSTOM_DATA_BLOB_OFFSET=v_KEYRING_CUSTOM_DATA_OFFSET,
            KEYRING_CUSTOM_DATA_BLOB_LENGTH=v_KEYRING_CUSTOM_DATA_LENGTH,
        )
        print(ret_cert.split("[ keyring_custom_data ]", 1)[1])
    return dedent(ret_cert)


if __name__ == "__main__":
    python_exe = 'python3'

    if os.name == 'nt':
        python_exe = 'python'

    BIN2C = f"{python_exe} {os.path.join('..', '..', '..', '..', '..', '..', '..', 'tools', 'bin2c', 'bin2c.py')}"
    my_parser = argparse.ArgumentParser(
        description="Creates a Public Key Certificate for (non-K3) HS-SE devices")

    my_parser.add_argument('--root_key',                type=str,
                           required=True, help='Customer MPK key')
    my_parser.add_argument('--keys_info',               type=str,
                           required=True, help='Keys info json file')
    my_parser.add_argument('--enckey',          type=str,
                           help='File with encryption key inside it')
    my_parser.add_argument('--sign_key_id', required=False, type=int, default=0,
                           help='key index of signing certificate')
    my_parser.add_argument('--enc_key_id', required=False, type=int, default=0,
                           help=' properties of key used for encryption of symmetric key blob')
    my_parser.add_argument('--kd_salt',        type=str,
                           help='Path to the salt required to calculate derived key from manufacturers encryption key')
    my_parser.add_argument('--rsassa_pss',
                           help='If binary needs to be signed RSASSA PSS scheme or not',  action="store_true")
    my_parser.add_argument('--pss_saltlen', type=int,   default=0,
                           help='Salt length for RSASSA PSS scheme',)
    my_parser.add_argument('--decrypt_mode', type=str,   default='CBC',
                           help='Decryption mode for decrypting key blob',)

    args = my_parser.parse_args()

    # create temp directory for temp files
    try:
        os.mkdir('tmpdir')
    except:
        None

    cert_str = get_cert(args)
    # print(cert_str)

    cert_file_name = "temp_cert"+str(randint(111, 999))

    with open(cert_file_name, "w+") as f:
        f.write(cert_str)

    cert_name = "x509_keyringcert_"+str(randint(111, 999))+".cert"

    # Generate the certificate
    if args.rsassa_pss:
        response = subprocess.check_output('openssl req -new -x509 -key {} -nodes -outform DER -out {} -config {} -{} -sigopt rsa_padding_mode:pss -sigopt rsa_pss_saltlen:{} '.format(
            args.root_key, cert_name, cert_file_name, g_sha_to_use, args.pss_saltlen), shell=True)
    else:
        response = subprocess.check_output('openssl req -new -x509 -key {} -nodes -outform DER -out {} -config {} -{}'.format(
            args.root_key, cert_name, cert_file_name, g_sha_to_use), shell=True)

    bin2c.binary_to_header(cert_name, 'keyringCert.h', 'CUST_KEYRINGCERT')

    # Delete the temporary files
    # remove temp directory
    try:
        shutil.rmtree('tmpdir')
    except:
        None
    os.remove(cert_file_name)
    # os.remove(cert_name)