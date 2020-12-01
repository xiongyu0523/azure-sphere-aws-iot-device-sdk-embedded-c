import json
import re
import boto3
from asn1crypto import pem, x509

ioTPolicyName = 'IoTSimplePolicy'
iotclient = boto3.client('iot')

def lambda_handler(event, context):

    certId = event['certificateId']
    
    response = iotclient.describe_certificate(certificateId=certId)
    pem_cert = response['certificateDescription']['certificatePem']
    certArn = response['certificateDescription']['certificateArn']

    region = re.search('iot:(.+?):', certArn).group(1)
    accountId = event['awsAccountId']

    type_name, headers, der_cert = pem.unarmor(pem_cert.encode('utf-8'))

    cert = x509.Certificate.load(der_cert)
    CNStr = cert.subject.native['common_name']

    response = iotclient.list_things(maxResults=1, attributeName='certCN', attributeValue=CNStr)
    thing = response['things']

    if not thing:
        response = iotclient.create_thing(thingName=f'Azure-Sphere-{CNStr[0:5]}', attributePayload={'attributes':{'certCN': CNStr, "certID": certId}})
        thingNameStr = response['thingName']

    else:
        thingNameStr = thing[0]['thingName']
        oldCertId = thing[0]['attributes']['certID']
        oldCertArn = f'arn:aws:iot:{region}:{accountId}:cert/{oldCertId}' 

        iotclient.update_thing(thingName=thingNameStr, attributePayload={'attributes':{"certID": certId}, 'merge':True})
        iotclient.detach_thing_principal(thingName=thingNameStr, principal=oldCertArn)
        iotclient.detach_policy(policyName=ioTPolicyName, target=oldCertArn)
        iotclient.update_certificate(certificateId=oldCertId, newStatus='INACTIVE')
        iotclient.delete_certificate(certificateId=oldCertId, forceDelete=True)

    iotclient.attach_thing_principal(thingName=thingNameStr, principal=certArn)
    iotclient.attach_policy(policyName=ioTPolicyName, target=certArn)
    iotclient.update_certificate(certificateId=certId, newStatus='ACTIVE')

if __name__ == "__main__":

    print('Only for testing')

    # define a dict to simulate MQTT payload for testing purpose
    payload = {
        'certificateId': '<certificate-id>', 
        'caCertificateId': '<ca-certificate-id>', 
        'timestamp': 1577248409400, 
        'certificateStatus': 'PENDING_ACTIVATION', 
        'awsAccountId': '075445498560', 
        'certificateRegistrationTimestamp': None
    }

    lambda_handler(payload, None)