import os
import json
import time
import boto3
from asn1crypto import pem, x509
from subprocess import run, PIPE

azsphere_program = r'C:\Program Files (x86)\Microsoft Azure Sphere SDK\Tools\azsphere '
ioTPolicyName = 'IoTSimplePolicy'
iamRolePath = '/service-role/'

region = boto3.session.Session().region_name
accountId = boto3.client('sts').get_caller_identity().get('Account')

iotclient = boto3.client('iot')
lambdaclient = boto3.client('lambda')
iamclient = boto3.client('iam')

CA_FILE_NAME = 'ca.cer'
VER_FILE_NAME = 'ver.cer'

def utility_verison():
    result = run(azsphere_program + 'show-version', capture_output=True)
    return result.stdout.decode('utf-8')

def utility_download_ca_certificate(file_name):
    result = run(azsphere_program + 'tenant download-ca-certificate -o ' + file_name, capture_output=True)
    str = result.stdout.decode('utf-8')
    return True if 'Saved the CA certificate' in str else False    

def utility_download_validation_certificate(code, file_name):
    result = run(azsphere_program + 'tenant download-validation-certificate -c ' + code + ' -o ' + file_name, capture_output=True)
    str = result.stdout.decode('utf-8')
    return True if 'Saved the validation certificate' in str else False    

def register_ca_certificate():

    if utility_verison() < '19.11':
        print('ERROR: update Azure Sphere SDK to the latest version')
        return

    response = iotclient.get_registration_code()
    regCode = response['registrationCode']

    if os.path.exists(CA_FILE_NAME):
        os.remove(CA_FILE_NAME)

    if os.path.exists(VER_FILE_NAME):
        os.remove(VER_FILE_NAME)

    if not utility_download_ca_certificate(CA_FILE_NAME):
        print('ERROR: failed to download ca certificate')
        return

    if not utility_download_validation_certificate(regCode, VER_FILE_NAME):
        print('ERROR: failed to download validation certificate')
        return

    with open(CA_FILE_NAME, 'rb') as f:
        ca_pem = pem.armor('CERTIFICATE', f.read())

    with open(VER_FILE_NAME, 'rb') as f:
        ver_pem = pem.armor('CERTIFICATE', f.read())

    try:
        response = iotclient.register_ca_certificate(caCertificate=ca_pem.decode('utf-8'), verificationCertificate=ver_pem.decode('utf-8'), setAsActive=True, allowAutoRegistration=True)
    except iotclient.exceptions.ResourceAlreadyExistsException:
        print("INFO: Already have this CA registered")

def create_iot_policy():
    try:
        response = iotclient.create_policy(
            policyName=ioTPolicyName, 
            policyDocument="{\"Version\":\"2012-10-17\",\"Statement\":[{\"Effect\":\"Allow\",\"Action\":\"iot:*\",\"Resource\":\"*\"}]}"
        )
    except iotclient.exceptions.ResourceAlreadyExistsException:
        print("INFO: Already have this IoT policy")

def create_lambda_rule(fn_name):

    role_name = fn_name + 'Role'

    assume_role_policy_document = json.dumps({
        "Version": "2012-10-17",
        "Statement": [{
            "Effect": "Allow",
            "Principal": {
                "Service": "lambda.amazonaws.com"
            },
            "Action": "sts:AssumeRole"
        }]
    })

    inline_policy_document = json.dumps({

        "Version": "2012-10-17",
        "Statement": [
            {
                "Effect": "Allow",
                "Action": [
                    "logs:CreateLogStream",
                    "logs:PutLogEvents"
                ],
                "Resource": "*"
            },
            {
                "Effect": "Allow",
                "Action": "logs:CreateLogGroup",
                "Resource": "*"
            },
            {
                "Effect": "Allow",
                "Action": "iot:*",
                "Resource": "*"
            }
        ]
    })

    try:
        response = iamclient.create_role(
            RoleName=role_name,
            AssumeRolePolicyDocument=assume_role_policy_document,
        )

    except iamclient.exceptions.EntityAlreadyExistsException:
        print('INFO: Already have this IAM role')
    else:
        iamclient.put_role_policy(RoleName=role_name, PolicyName='inline_policy', PolicyDocument=inline_policy_document)

    # wait for a while to sync 
    time.sleep(15)

    try:
        response = lambdaclient.create_function(
            FunctionName=fn_name,
            Runtime='python3.7',
            Role=f'arn:aws:iam::{accountId}:role/{role_name}',
            Handler=f"{fn_name}.lambda_handler",
            Code={'ZipFile': open(f'{fn_name}.zip', 'rb').read()},
        )

    except lambdaclient.exceptions.ResourceConflictException:
        print('INFO: Already have this Lambda function')

    rule_name = fn_name + 'Rule'

    iotclient.create_topic_rule(
        ruleName=rule_name, 
        topicRulePayload={
            'sql': 'SELECT * FROM \'$aws/events/certificates/registered/+\'', 
            'actions': [{'lambda':{'functionArn': f'arn:aws:lambda:{region}:{accountId}:function:{fn_name}'}}], 
            'ruleDisabled': False, 
            'awsIotSqlVersion': '2016-03-23'
        }
    )

    try:
        lambdaclient.add_permission(
            FunctionName=fn_name,
            StatementId='abcdefg',
            Action='lambda:InvokeFunction',
            Principal='iot.amazonaws.com',
            SourceArn=f'arn:aws:iot:{region}:{accountId}:rule/{rule_name}'
        )
    except lambdaclient.exceptions.ResourceConflictException:
        print('INFO: Already have this permission added')

if __name__ == "__main__":

    register_ca_certificate()
    create_iot_policy()
    create_lambda_rule('AzureSphereJITR')