import json
import base64
import time

import boto3
from botocore.exceptions import ClientError

from cryptography.fernet import Fernet
from cryptography.hazmat.backends import default_backend
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

from utils import lambdaResponse as response
from utils import get_timestamp

def download(event, context):
    """
    Return the encrypted log file and plain text session key
    
    event dictionary input elements:
     - SHA256 digest of the binary file

    """
    #load the event body into a dictionary
    body = json.loads(event['body'])

    # Test to be sure the necessary elements are present
    try:
        assert 'serial_num' in body 
        assert 'digest' in body
    except AssertionError:
        return response(400, "Missing required parameters.")
    
    #Determine the identity of the requester.
    requester_data = event["requestContext"]
    if requester_data["authorizer"]["claims"]["email_verified"]:
    	identity_data = event["requestContext"]["identity"]
    	ip_address = identity_data["sourceIp"]
    	email = requester_data["authorizer"]["claims"]["email"].lower()
    else:
    	return response(400, "Email not verified.")

    # Lookup the data needed from the unique CAN Logger by its serial number
    dbClient = boto3.resource('dynamodb', region_name='us-east-2')
    table = dbClient.Table("CANLoggers")
    try:
        item = table.get_item( 
            Key = {'id': body['serial_num'],} 
        ).get('Item')
    except:
        return response(400, "Unable to retrieve serial number from table.")
    
    # load the device's public key which was stored as a base64 encoded binary
    device_public_key_bytes = bytearray.fromhex(base64.b64decode(item['device_public_key']).decode('ascii'))
    device_bytes = b'\x04' + device_public_key_bytes
    device_public_key = ec.EllipticCurvePublicKey.from_encoded_point(ec.SECP256R1(),device_bytes)
    
    # Decrypt the data key before using it
    cipher_key = base64.b64decode(item['encrypted_data_key'])
    data_key_plaintext = decrypt_data_key(cipher_key)
    if data_key_plaintext is None:
        return response(400, "Data Key is Not Available")

    # Decrypt the private key for the device
    f = Fernet(data_key_plaintext)
    decrypted_pem = f.decrypt(base64.b64decode(item['encrypted_server_pem_key']))
    
    #load the serialized key into an object
    server_key = serialization.load_pem_private_key(decrypted_pem, 
                                                    password=None, 
                                                    backend=default_backend())

    #Derive shared secret
    shared_secret = server_key.exchange(ec.ECDH(),device_public_key)
    
    #look up session key
    print('digest: {}'.format(body['digest']))
    table = dbClient.Table("CanLoggerMetaData")
    try:
        item = table.get_item( 
            Key = {'digest': body['digest'],} 
        ).get('Item')
    except Exception as e:
        return response(400, "File Meta data not availalble. Please upload file.\n{}".format(repr(e)))

    #Check if email is the uploader or has share access
    if not email == item['uploader'] and not email in item['access_list']:
        return response(400, "You do not have permission to download this file.")

    session_key = bytearray.fromhex(item["session_key"])
    
    #use the first 16 bytes (128 bits) of the shared secret to decrypt the session key
    cipher = Cipher(algorithms.AES(shared_secret[:16]), 
                                   modes.ECB(), 
                                   backend=default_backend())
    decryptor = cipher.decryptor()
    clear_key = decryptor.update(session_key) + decryptor.finalize()
    print(clear_key.hex().upper())

    #Get encrypted log file
    s3_client = boto3.client('s3')
    try:
        #obj = s3.Object('can-log-files',body['digest'])
        #https://boto3.amazonaws.com/v1/documentation/api/latest/reference/services/s3.html#S3.Client.generate_presigned_url
        log_file_url = s3_client.generate_presigned_url('get_object',
                                                    Params={'Bucket': 'can-log-files',
                                                            'Key': body['digest']},
                                                    ExpiresIn=3600)
        print(log_file_url)
    except Exception as e:
        print(e)
        return response(400, "Log file cannot be found in s3 Bucket")

    encoded_clear_key = base64.b64encode(clear_key).decode('ascii')
    data = {'log_file':log_file_url,'session_key':encoded_clear_key}

    # set attribution data
    timestamp = get_timestamp(time.time())
    access_tuple = str((timestamp, email, ip_address))
    print("Access Tuple: {}".format(access_tuple))
    download_list = item["download_log"]
    download_list.append(access_tuple)

    #update the download log with the user details.
    table.update_item(
        Key = {'digest':body['digest']},
        UpdateExpression = 'SET download_log= :var',
        ExpressionAttributeValues = {':var':download_list},
        )

    return response(200, data)
    
def decrypt_data_key(data_key_encrypted):
    """Decrypt an encrypted data key

    :param data_key_encrypted: Encrypted ciphertext data key.
    :return Plaintext base64-encoded binary data key as binary string
    :return None if error
    """

    # Decrypt the data key
    kms_client = boto3.client('kms')
    try:
        response = kms_client.decrypt(CiphertextBlob=data_key_encrypted)
    except ClientError as e:
        print(e)
        return None

    # Return plaintext base64-encoded binary data key
    return base64.b64encode((response['Plaintext']))