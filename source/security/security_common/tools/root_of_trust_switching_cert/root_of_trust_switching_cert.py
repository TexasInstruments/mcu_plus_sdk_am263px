# Script to add x509 certificate to input binary (SBL or hsmRt)
# The certificate used will be ROM format
#
# Python 3 script

import argparse
import os
import sys
import subprocess
import binascii
from re import sub
from random import randint
import shutil
from textwrap import dedent

g_sha_to_use = "sha512"

g_sha_oids = {
    "sha256": "2.16.840.1.101.3.4.2.1",
    "sha384": "2.16.840.1.101.3.4.2.2",
    "sha512": "2.16.840.1.101.3.4.2.3",
    "sha224": "2.16.840.1.101.3.4.2.4",
}

g_openssl111_x509_template = '''
[ req ]
distinguished_name     = req_distinguished_name
x509_extensions        = v3_ca
prompt                 = no

dirstring_type = nobmp

[ req_distinguished_name ]
C                      = US
ST                     = SC
L                      = New York
O                      = Texas Instruments., Inc.
OU                     = SITARA MCU
CN                     = Albert
emailAddress           = Albert@gt.ti.com

[ v3_ca ]
basicConstraints = CA:true
1.3.6.1.4.1.294.1.1=ASN1:SEQUENCE:boot_seq

[ boot_seq ]
certType     =  INTEGER:{CERT_TYPE}
'''

g_openssl3_x509_template_inner = '''
[ req ]
distinguished_name     = req_distinguished_name
x509_extensions        = v3_ca
prompt                 = no

dirstring_type = nobmp

[ req_distinguished_name ]
C                      = US
ST                     = SC
L                      = New York
O                      = Texas Instruments., Inc.
OU                     = SITARA MCU
CN                     = Albert
emailAddress           = Albert@gt.ti.com

[ v3_ca ]
basicConstraints = CA:true
subjectKeyIdentifier = none
1.3.6.1.4.1.294.1.1=ASN1:SEQUENCE:boot_seq

[ boot_seq ]
certType     =  INTEGER:{CERT_TYPE}
bootCore	 =	INTEGER:0
bootCoreOpts =	INTEGER:0
destAddr	 =	FORMAT:HEX,OCT:00000000
imageSize	 =	INTEGER:0
'''

g_openssl3_x509_template_outer = '''
[ req ]
distinguished_name     = req_distinguished_name
x509_extensions        = v3_ca
prompt                 = no

dirstring_type = nobmp

[ req_distinguished_name ]
C                      = US
ST                     = SC
L                      = New York
O                      = Texas Instruments., Inc.
OU                     = SITARA MCU
CN                     = Albert
emailAddress           = Albert@gt.ti.com

[ v3_ca ]
basicConstraints = CA:true
subjectKeyIdentifier = none
1.3.6.1.4.1.294.1.1=ASN1:SEQUENCE:boot_seq
1.3.6.1.4.1.294.1.2=ASN1:SEQUENCE:image_integrity

[ boot_seq ]
certType     =  INTEGER:{CERT_TYPE}
bootCore	 =	INTEGER:0
bootCoreOpts =	INTEGER:0
destAddr	 =	FORMAT:HEX,OCT:00000000
imageSize    =  INTEGER:{IMAGE_LENGTH}

[ image_integrity ]
shaType     = OID:{SHA_OID}
shaValue    = FORMAT:HEX,OCT:{SHA_VAL}
'''


def get_sha_val(f_name, sha_type):
    sha_val = subprocess.check_output(
        'openssl dgst -{} -hex {}'.format(sha_type, f_name), shell=True).decode()
    return sub("^.*= ", r'', sha_val).strip('\n')


def get_cert_inner():
    # Primary Certificate signed against SMPK
    certType = 0xA5000005
    ret_cert_inner = ""
    openssl_version: str = str(
        subprocess.check_output(f"openssl version", shell=True))

    if "1.1.1" in openssl_version:
        print(
            f"WARNING: OpenSSL version {openssl_version.split()[1]} found is not recommended due to EOL. Please install version 3.x .")
        ret_cert_inner = g_openssl111_x509_template.format(
            CERT_TYPE=certType,
        )

    elif "3." in openssl_version:
        print(f"INFO: OpenSSL version {openssl_version.split()[1]} found.")
        ret_cert_inner = g_openssl3_x509_template_inner.format(
            CERT_TYPE=certType,
        )

    else:
        print(
            f"ERROR: OpenSSL version {openssl_version.split()[1]} found is not compatible. Please install version 1.1.1 or 3.x to continue.")
        sys.exit()
    return dedent(ret_cert_inner)

def get_cert_outer(cert_name_inner):
    # Secondary Certificate which contains the SHA 512 value of the primary certificate and is signed against BMPK
    certType = 0xA5000006
    sha_val = get_sha_val(cert_name_inner, g_sha_to_use)
    inner_cert_length = os.path.getsize(cert_name_inner)
    ret_cert_outer = ""
    openssl_version: str = str(
        subprocess.check_output(f"openssl version", shell=True))

    if "1.1.1" in openssl_version:
        print(
            f"WARNING: OpenSSL version {openssl_version.split()[1]} found is not recommended due to EOL. Please install version 3.x .")
        ret_cert_outer = g_openssl111_x509_template.format(
            CERT_TYPE=certType, IMAGE_LENGTH = inner_cert_length, SHA_OID = g_sha_oids[g_sha_to_use], SHA_VAL = sha_val
        )

    elif "3." in openssl_version:
        print(f"INFO: OpenSSL version {openssl_version.split()[1]} found.")
        ret_cert_outer = g_openssl3_x509_template_outer.format(
            CERT_TYPE=certType, IMAGE_LENGTH = inner_cert_length, SHA_OID = g_sha_oids[g_sha_to_use], SHA_VAL = sha_val
        )
    else:
        print(
            f"ERROR: OpenSSL version {openssl_version.split()[1]} found is not compatible. Please install version 1.1.1 or 3.x to continue.")
        sys.exit()
    return dedent(ret_cert_outer)


# MAIN
my_parser = argparse.ArgumentParser(
    description="Creates a RoT Switching Certificate")
my_parser.add_argument('--smpk',    type=str, required=True,
                       help='Path to the smpk signing key to be used while creating the certificate')
my_parser.add_argument('--bmpk',    type=str, required=True,
                       help='Path to the bmpk signing key to be used while creating the certificate')

args = my_parser.parse_args()

cert_str_inner = get_cert_inner()

cert_file_name_inner = "temp_cert_inner"+str(randint(111, 999))

with open(cert_file_name_inner, "w+") as f:
    f.write(cert_str_inner)

cert_name_inner = "rot_switch_inner" + str(randint(111, 999))

# Generate the inner certificate
subprocess.check_output('openssl req -new -x509 -key {} -nodes -outform DER -out {} -config {} -{}'.format(
    args.smpk, cert_name_inner, cert_file_name_inner, g_sha_to_use), shell=True)

cert_str_outer = get_cert_outer(cert_name_inner)
# print(cert_str_outer)
cert_file_name_outer = "temp_cert_outer"+str(randint(111, 999))

with open(cert_file_name_outer, "w+") as f:
    f.write(cert_str_outer)

cert_name_outer = "rot_switch_outer"
try:
    os.remove(cert_name_outer)
except OSError:
    pass
# Generate the outer certificate
subprocess.check_output('openssl req -new -x509 -key {} -nodes -outform DER -out {} -config {} -{}'.format(
    args.bmpk, cert_name_outer, cert_file_name_outer, g_sha_to_use), shell=True)

final_fh = open("rot_switch_cert.cert", 'wb+')
cert_fh_inner = open(cert_name_inner, 'rb')
cert_fh_outer = open(cert_name_outer, 'rb')
cert_data_outer = cert_fh_outer.read()
cert_data_inner = cert_fh_inner.read()
# print(cert_data_inner,len(cert_data_inner))
# print(cert_data_outer,len(cert_data_outer))
temp_cert = cert_data_outer + cert_data_inner
final_fh.write(temp_cert)

cert_fh_inner.close()
cert_fh_outer.close()
final_fh.close()

# Delete the temporary files
os.remove(cert_file_name_outer)
os.remove(cert_file_name_inner)
os.remove(cert_name_inner)
os.remove(cert_name_outer)