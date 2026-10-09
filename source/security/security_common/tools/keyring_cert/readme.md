# Keyring Cert-Gen tool {#TOOLS_KEYRING_CERT_GEN}

# Description:
- This script is used to generate X.509 certificate for keyring.
- Make sure that python3 and its dependent modules are installed in the host machine.
- keyring_cert_gen.py parses json object containing the following meta-data for keyring cert generation:
    - keyring_sw_rev: This integer value is used for anti-rollback check against the keyring software revision available in device efuses.
    - keyring_ver: Keyring version is of type integer and is the version which is supported against the TIFS-MCU version being used in HSM.
    - num_of_asymm_keys: Number of Asymmetric keys being imported.
    - num_of_symm_keys: Number of Symmetric keys being imported.
    - num_of_private_asymm_keys: Number of Asymmetric private keys being imported.
    - keyring_custom_data_available: Denotes whether a custom data is being imported or not.
    - keyring_asymm: Array of asymmetric keys with respective key rights and hash algorithm to compute and store the hash of the public key.
    - keyring_symm: Array of symmetric keys with respective parameters.
    - keyring_private_asymm: Array of asymmetric private keys with respective parameters.
    - keyring_custom_data: Arrayc of custom data with offset and respective data to be imported.

\imageStyle{keyring_tooling_view.png,width:50%}
\image html keyring_tooling_view.png "Keyring certificate generation and import"

\code
{
    "keyring_sw_rev": 1,
    "keyring_ver": 1,
    "num_of_asymm_keys": 7,
    "num_of_symm_keys": 3,
    "num_of_private_asymm_keys": 1,
    "keyring_custom_data_available": "yes",
    "keyring_asymm": [
        {
            "key_id": "0",
            "key_rights": "000000AA",
            "pub_key": "aux_keys/public2k.pem",
            "hash_algo": "SHA256"
        },
        {
            "key_id": "1",
            "key_rights": "000000AA",
            "pub_key": "aux_keys/public3k.pem",
            "hash_algo": "SHA512"
        },
        {
            "key_id": "2",
            "key_rights": "000000AA",
            "pub_key": "aux_keys/public4k.pem",
            "hash_algo": "SHA512"
        },
        {
            "key_id": "3",
            "key_rights": "000000AA",
            "pub_key": "aux_keys/prime265v1_public.pem",
            "hash_algo": "SHA512"
        },
        {
            "key_id": "4",
            "key_rights": "000000AA",
            "pub_key": "aux_keys/secp384r1_public.pem",
            "hash_algo": "SHA512"
        },
        {
            "key_id": "5",
            "key_rights": "000000AA",
            "pub_key": "aux_keys/secp521r1_public.pem",
            "hash_algo": "SHA384"
        },
        {
            "key_id": "6",
            "key_rights": "000000AA",
            "pub_key": "aux_keys/brainpoolp512r1_public.pem",
            "hash_algo": "SHA384"
        }
    ],
    "keyring_symm": [
        {
            "key_id": "0",
            "key_rights": "0000000A",
            "key_length": 128,
            "aes_key": "aux_keys/aes128.key"
        },
        {
            "key_id": "1",
            "key_rights": "0000000A",
            "key_length": 192,
            "aes_key": "aux_keys/aes192.key"
        },
        {
            "key_id": "2",
            "key_rights": "0000000A",
            "key_length": 256,
            "aes_key": "aux_keys/aes256.key"
        }
    ],
    "keyring_private_asymm": [
        {
            "key_id": "0",
            "key_rights": "000000AA",
            "key_type": "BRP512",
            "private_key": "aux_keys/brainpoolp512r1_private.pem"
        }
    ],
    "keyring_custom_data": [
        {
            "offset": 0,
            "custom_data": "aux_keys/brainpoolp512r1_private.pem"
        }
    ]
}

\endcode
- Each asymmetric key json object contain 4 fields:
    - key_id: The identifier serves as a distinct number for the keyring, employed by various services including debug authentication and proc auth boot. The actual storage location will be `AUXILIARY_START_INDEX + key_id`. This value can be given by the user, please be cautious as it might over-write if a key is already present at the index `32 + key_id`.
    - key_rights: Integer value in HEX format which signifies the rights associated with the key.
        - debugAuth (0-3b) - flag to indicate key right for debug certificate authentication
        - imageAuth (4-7b) - flag to indicate key right for application image authentication
        - For example: 0x000000AA represents the public key hash can be used for both imageAuth and debugAuth
    - pub_key: Location of the public key
    - hash_algo: Hash algorithm used for hashing public key.
        - SHA256, SHA384 and SHA512 are valid hash algorithms.
- Each Symmetric key json object contain 4 fields:
    - key_id: The identifier serves as a distinct number for the keyring. The actual storage location will be `AUXILIARY_START_INDEX + key_id`. This value can be given by the user, please be cautious as it might over-write if a key is already present at the index `32 + key_id`.
    - key_rights: Integer value in HEX format which signifies the rights associated with the key.
        - imageDecrypt (0-3b) - flag to indicate key right for decryption during secure boot.
        - For example: 0x0000000A represents the key can be used for image decryption during secure boot.
    - key_length: Length of the key in bits. (Valid values: 128, 192, 256)
    - aes_key: Location of the symmetric key
- Each Aymmetric Private key json object contain 4 fields:
    - key_id: The identifier serves as a distinct number for the keyring. The actual storage location will be `key_id`. This value can be given by the user, please be cautious as it might over-write if a key is already present at the index `key_id`.
    - key_rights: Integer value in HEX format which signifies the rights associated with the key.
    - key_type: Integer value denotes the key type (e.g., RSA, ECC curve identifier).
    - private_key: Location of the asymmetric private key
- Each Custom Data json object contain 2 fields:
    - offset: The offset of the custom data in the certificate.
    - custom_data: Path to custom data to be imported.
- keyring_cert_gen.py expects 2 mandatory arguments:
    - root_key: certificate is signed using customer MPK and expects path to customer active ROT.
    - keys_info: json file with keyring meta data.
- A dummy json and keyring certificate header file is also availble in ${TIFS_INSTALL_PATH}/tools/keyring_cert for reference.
- Below are the logs after execution of script
    
\code
keyring version = 1
keyring software revision = 1
Number of asymmetric keys = 7
Number of symmetric keys = 3
Number of private asymmetric keys = 1
Custom Key Data available = yes

[ keyring_asymm ]
asymm_key0=SEQUENCE:comp0
asymm_key1=SEQUENCE:comp1
asymm_key2=SEQUENCE:comp2
asymm_key3=SEQUENCE:comp3
asymm_key4=SEQUENCE:comp4
asymm_key5=SEQUENCE:comp5
asymm_key6=SEQUENCE:comp6

[ comp0 ]
keyId=INTEGER:0
key_rights=FORMAT:HEX,OCT:000000AA
hash_algo=INTEGER:6
public_key=FORMAT:HEX,OCT:582fc2a6cc997d2df6c0299aeef0a7b606d3fcc2261dfade8c2684c6663ab50b

[ comp1 ]
keyId=INTEGER:1
key_rights=FORMAT:HEX,OCT:000000AA
hash_algo=INTEGER:4
public_key=FORMAT:HEX,OCT:6e430c4a09572240f9acd4b4777b2d662fe28c73ef475280d8a4e7c766d13680b14549a84e6e6c7b69a0ba33840dc9f51f110afb156251a6139c26693e6d4ddf

[ comp2 ]
keyId=INTEGER:2
key_rights=FORMAT:HEX,OCT:000000AA
hash_algo=INTEGER:4
public_key=FORMAT:HEX,OCT:67cd4b91e7966af635a53642102abb2c8fdab1a47277c88eb4ec3e1a6ce5155a2325f66b3d392f3f5091e526057cd5e6b6d5085ae46f169c61de77c7dffbb238

[ comp3 ]
keyId=INTEGER:3
key_rights=FORMAT:HEX,OCT:000000AA
hash_algo=INTEGER:4
public_key=FORMAT:HEX,OCT:79bd183d942cbf1542a2bad2a469242392c515c1546fc8a43e86a024097ba4bb4db65ccade7fb6b252deccd255491151df927f998d468153072cf3e4949d6002

[ comp4 ]
keyId=INTEGER:4
key_rights=FORMAT:HEX,OCT:000000AA
hash_algo=INTEGER:4
public_key=FORMAT:HEX,OCT:4a8959978336886acbe3cf294b1b8e1aa68f1cb836d78aa11332a57cb44c435c82fc672901a2a242e5169543502106ac0cda1b04a9769723154d640e832298ba

[ comp5 ]
keyId=INTEGER:5
key_rights=FORMAT:HEX,OCT:000000AA
hash_algo=INTEGER:2
public_key=FORMAT:HEX,OCT:1b759f0c0b04796cc599033bd58e33730ce6b2380f7442fe030beffa3cc844ce0d196f48e5ebeb6d88eea0f7ddc3beb7

[ comp6 ]
keyId=INTEGER:6
key_rights=FORMAT:HEX,OCT:000000AA
hash_algo=INTEGER:2
public_key=FORMAT:HEX,OCT:b502e951a5f5ed4bc99191511b530597d3a2d356d0f83887a54253a7ec46fb2c081c7d39391f846932357a57133f8c11


[ symm_key0 ]
key_id=INTEGER:0
key_rights=FORMAT:HEX,OCT:0000000A
key_length=128 bits (16 bytes)
aes_key=FORMAT:HEX,OCT:b90f6f0f39d1f12a7164ea6da10899ff00000000000000000000000000000000

[ symm_key1 ]
key_id=INTEGER:1
key_rights=FORMAT:HEX,OCT:0000000A
key_length=192 bits (24 bytes)
aes_key=FORMAT:HEX,OCT:d1caa2fd0fdca6098c1e9a56b272e5a157c7bf67181669000000000000000000

[ symm_key2 ]
key_id=INTEGER:2
key_rights=FORMAT:HEX,OCT:0000000A
key_length=256 bits (32 bytes)
aes_key=FORMAT:HEX,OCT:7defde6c464a0e96e9d6636d1672cf1cc82a3efb5de88c0a507c3d75eb0fe692

[ asymm_private_key0 ]
key_id=INTEGER:0
key_rights=FORMAT:HEX,OCT:000000AA
key_type=FORMAT:HEX,OCT:1
key_length=221 bytes
asymm_private_key=FORMAT:HEX,OCT:3081da0201010440131495c389b5c6c1cac497504d7d83724c23df68a534e1f8bacaf1839abd0f0754a35f742f775763ab022d987cbbbbe9b11b2b38a080fe10538b378c583c8ea0a00b06092b240303020801010da18185038182000498c092f78ac6e6956a8602e96f856cc12cf655231134b89d867bcdabcf8dacf29844e9526c207e0637102efe9c29f4932b06ca10be96666b906cce67f0ca68ca16f37c54bd5c160bc8524ee597eea3ba77e8f9df542165391edcb65abbaba592f223be608fd25e90f2580fc0c2ea698796e22b27ea643120336f6fa72281981f

[ keyring_custom_data ]
offset=INTEGER:0
custom_data=FORMAT:HEX,OCT:2d2d2d2d2d424547494e2045432050524956415445204b45592d2d2d2d2d0a4d4948614167454242454154464a584469625847776372456c31424e66594e7954435066614b553034666936797647446d7230504231536a583351766431646a0a717749746d487937752b6d78477973346f49442b45464f4c4e347859504936676f4173474353736b41774d43434145424461474268514f42676741456d4d43530a393472473570567168674c706234567377537a3256534d524e4c6964686e764e71382b4e72504b59524f6c536243422b426a63514c7636634b6653544b77624b0a454c36575a6d7551624d356e384d706f7968627a664653395842594c79464a4f355a66756f37703336506e665643466c4f523763746c71377136575338694f2b0a59492f535870447957412f4177757070683562694b7966715a4445674d32397670794b426d42383d0a2d2d2d2d2d454e442045432050524956415445204b45592d2d2d2d2d0a
custom_data_length=361 bytes

key_blob=FORMAT:HEX,OCT:b48405c6bf44fb6b2d3acad08027d8c6272f10d756e9ef80b8ae65cef264c026e517bb45effed2ff1cfde66b104bb298c0060707c427f2c81671da589253f08f0a5b4dc105a2fb446d7c28c9d7b138502a3abd2190ab5a3cbf083d1ce918b6117e58737d749da25969b61abd3d2f179c124da217aacbbc868d724f7a6ebb9f87db7916808c985e31cffe5f05f717c60ca2eac3071f142041b053c450f36442396643a1534367e4921e8c3a5f421fa8dd
inital_vector =  FORMAT:HEX,OCT:ae7efeea01aa23b678132afc0851b3af
random_string =  FORMAT:HEX,OCT:afbd820e32b6bb3dd656cfd8d74cab16b003a8469f34bfb9a4c492c361be07e9
enc_key_salt  =  FORMAT:HEX,OCT:acca65ded29296fea498ab8a9a15aaa27445ab7c75757c99125254619e4a513b
enc_key_id    =  INTEGER:0
decryption_mode =  INTEGER:1


key_blob=FORMAT:HEX,OCT:cc7a46dc1c7765cdea3d21fb511893915bb5dcd3714634d37e8a017873d6ae509a3e6dfdbf5f1368d4b896c607a0d5255ac5b3df8dedb347aa85320858c04e5d95c8e6343347b794d7d0f7686b93cfbfb5e8796bca4ba7a1653285134372dbc66d9ab623739c613c7330bd91d8cb71f2fc3532059f610b5c863123f8a4ee46189d226c2983391a7bdd872d1d9645abeef32c78d1715845408afb9c3a71c1ce73f5e8d0921fa3b2077678155ba7d75da2b927557ac7747119a5cf562944f70ac09c0c5bdfd6521cd38d1b10bff6a6bd2213e885c41397fa1958c05fe36648ba8aef142f2f28d4f5fda1c36c1c9bfc259e1d9844f9e4442481044b73674ce3ff929969f44cb099dd7049f6138050bf91830e9e33b5d8f70bdbd2e73ab81f55ea490588e5c430388710ffb0fd22f878705d8f8ebe5c8fd4a613af70fcccfdffeabf1ab8787e4452c08d73dfc8a42c558e7d665928df32a36e7a1ae4d04ced8cf43e06d49f1b975d0d7a7a782442761cdc065a84c5b195db4b99b55914b314a31e24c26c6c001067b6597f3836700d302a1b54998c23e9c094c3c0ae0753a2c380e01cf8a0034c5cc4e42277d36f5e6a224f21ea6bca488c46b55e66f2e585e6acccb340915af68134f590bc51e2615380a9c4d76e439917441f78e0aafa790298ef24c25392ff1cd780bb8ce8819785752e3b044d4c1ce5fe11699fe9044ad057c803b12ef56486a620f104488f83dbb22a7efc632194e7c92a0188090adb3234ccf3ad5c2a3a59c56942403259b172041aed110f7320640dedc1bacb3c793aa1f6378732c972eb70a1aef5488b8bd884118d64a705b88f870f9a5e528066e32356351d3f2601734794a7c1dd5dc094daf2d74214a10d18169147362229d201c118dda7c2effa23ccd74687f816c1a45836d847d427de97a5a8177215817655b616812a91ae08496db37fcb9e9df49c048a001cf6d26b9c12911278cb438db670aac34f928a9b38fb235afaf129d172855572cc999c2228142050674452b832ea2a9d592529d0a5f322950ebe6689d731f400c80a18c79c23e77f9d7270c4d36e1cdc532f134a03ee237fecffa0d085c2afc22c95a72b2a658ef0c552e981c7d07d262fd75268600949dd4b3d68824e76ab2260169a4c8462d5529360b2a8fa03f1e4d140e298bf099aabf643402c699ac2c19148972bb5ec55e2b307b6bb9a499549d598a2e5f243b04546acca4787ea2a1ea6fc94c8f28b2c602edff4f9de06e4661afc231c4a89b1c39f08ebe3706b2aa799385425a767393238caed7092b915e4d99e3c39aa0059018372cefd2d7e64ab65951bfe45f81f0bb4172984c1b3a42baf4a72522cebc0087ec5e29841bd46c0befcd48da4bbc0b56844600d1dccb3ce16d21ab9d5deef23e099d671fd59c08f7a07bef8c539832b46a992ddffef94c6aa0324a1537afdd126e0ad73ce2e64200d8f19b2145bf4d1107dd04377d82a22222cb55098ab9c7137a56b0b9486ebcdd7b0808cda1b9a56087cc80a9de12cb7d2b1418f6f313bc0af1666daeeeea8257ce50464c1a91740b2ceecf5f8fcfd118cb356684ac8a19a6d374a9d463974f29f431366343704e938235202303e2baeed1fffc37b26a7adcef8f4ba7a780863c8aa8ed8641dc3c13b4df53fb10f4dfb6ca4533bdf98939e85b58f79dc2f1a685e7d5c6224b31ffdcb222dc17770286469f16954caccd8fefe02bbe10d6025f41638f1e0613c5f085b471f2c2c384c1ab310b556e0e05e73b2993e019da0d2b90ee6dcc07fe6a6208f3a8bca8c93fbf4cf338c6b35e9a39b7d862652e9d3c40cb716faf5fd3c456da144f8c8c54253620988e29bc3f8245d00d46a4338ee63530d8a0d444830e4eb6403b3a46a7f02323905613a382bd540f6f81aad00e09e38ebe649fe7b641ae1640229b8657b13326a6b324d5eb6a44cc83dc3e1ab6d17af1a9cf8ee082019adeb576ad9b129cc7a109d100843907e802592f6950e02db2c1aae38d72694f97e6d3d5c14c0c22e73754b25acbea54d76a3d4f1dbaabf2563705c90ad75798a91738504cfc63b88d8b85009d550d2838f2a69f704a850d90de11ea97b5f82d0ed2efc843d54123e0cc6dfa1564cb34866e23a03f79229bc1fc07beb4a02325ecab54ec298d3140d7f128f4ef5e16e63828adff1ff27c2937eb2732eecf19c218aab60a628502a288b329fc8127215d29dd4de1cca3c1606ca0df4476f43099c718112f81b3ca1573106e0d917fbc47c3759b50bd2f13ba666604c0ad2e2e56a855c9766ff3ab54282c2b5737f511169309ab5ae68b7c86763d49b26f74819890c26a10cdbc32f80b03d352360a08be1038162a27f04458c3f7dd46a53559d01495f31c81201d58c4bd97735a7a8cbe5b9a0b74f855c7ce8c809673b237dded8ad2e7c610d0bb581266000efe37a8694db3248e08b347375b3b3a7b04a277b0f2e7ab27eef36a0862b055c12f188b75084e95989f81897061491a8609d6ef11ee1327d9060b2ef6572b0ab08d1a8b0fc7f94a3814e3a3d495055caf7145df5b9048fda90e1889a530b1fb8dc239a955c63b7611bc2e31c79943e40a54792fecf7ae54d6d8db5217157476e8f690ceee6db43294d3e45224ec4d8ce1524eb3e00c937d392e2c51f00866c5a810d863cba8c601f83e7faeff4a4e9b389074cda522aa550680460eeb64844522a4a5f62f83263c470169e44f2ae4a0c8bcd1f1be4b081796a6587973af362be6a72cd6bc3a3173cbb08aad2ae2fd8c048ac7b4dbb9dd7ceb7b6857a4dd83a168b2b9167e4e6184540d66bf268ea76d2c809701ce19f628b6997a872d40345d9ea6324a514b920a1bf3d0a8e5732ce264695854e056da25bb31880c9c4e20a5b3a9b5b73039729baffe22681957ac885e7db756d00f68d09652aec3db6c42c783623201dd8dbe9e22925f5d95ce9a3ce27e2e6890c0e327090389d62761466605ca46a85f871ea23fb9451341cde08c8db1939b857b74940f9b57e08946c9c39aea3a4adeaec71fe692a63d4091ea3ca41aebe35255a928b8b59c802673a5260db694aa57abe0ac86342a58f37ebb6550ce7fc45e523a988282763cf700c3765c428abbda170c8d12981bf8ff69063dd699f3834b5905c9a6c720ee1540da3d2200927ac486a20d33018a5aa136d9c80cd6db5dd6f8686ed84c14454a7bac31d3ba9e02a689d8c45bd406056758ad03f4cafecabb2d4ee08f6a08abba2bd8516163d9fd1b4df5cbbf3716a53bd9299707c6b407bb3896d0bf3985456df693ddf2b85b4baee40ffbb22d56c59ba6a05a8c4f46294ddd37e21ed61751ffe563517c6f637625535d77b317ef722560c246e9cb22ea97447ad991d7e467b93068a2504a838c488b408b26f92d157adc69a244e5f2cfc0a6eb7a39edfe84b807da29ef3bb0581aa0f7fdcdb31a9ba19c270cc577ca99b5bbecd4bc1dbe85c5322283e7d3a9c62181af01c42cbf091d131ad90e5f4509c3669c6d3f3d05550288c6c062511a747962b41add3c3aeea01cf5178fe57333ec6c3d36b7c08969da4c213ea212a0d583ac4c5f9d832c9c23c0b12b55457cdfb779ff29e8d4e75b937
inital_vector =  FORMAT:HEX,OCT:51020bbcae02d7767468eba91e3b8936
random_string =  FORMAT:HEX,OCT:7dcddd4cce1ad4b726b63e309dd5fdeac7c67834a2c6795c6328fc03c936e384
enc_key_salt  =  FORMAT:HEX,OCT:acca65ded29296fea498ab8a9a15aaa27445ab7c75757c99125254619e4a513b
enc_key_id    =  INTEGER:0
decryption_mode =  INTEGER:1


key_blob=FORMAT:HEX,OCT:685628c212533f50774171069b6fc3ceec53b9759f47e18ac0ea592eb6372a2ba583021660afe5cc9d609a9890f4200308dc483c3ad8663279b824b2158820c06e1ea5e1958882e0b50c166c52d9585d83cb5c7b1a354f0131d338428f1ccf834d662304a1e224fd46d910454ad56db57afa1d3b45e3a3938609a2852675f28c0a4fe20e932e838040907555f2d9659c74c20a427c59c6e54fd2e78818c2dfec520f4effa80740972fbef9b7cc73aee467aa6f578f0df5776a35e09e2d2d91baecb1a044d9c5fd8925ef4a126544486ecd4d6239965af69f8ee33b08185e6a7361762d66839048a36ea492b1485e7ea3b6f5fa67f82449ae8df88205c1a690be40d9f5af707ced57086c2edb0575e43e5fd8c183dde17f1f66bd48c7324808fcfff7105b5a095a9f9b9c341f4028c2e4ffa183f6844405cd7c0c5e7470895c9b8e95f8aa5649ac7122f7c364f80ef0781463d489aa4a8830d5e08ab762e997f5136088d5df9964fc522eb5ae475abf876211ae6a4f9e9bfd33acb50f4ed333127744f1a304dda63b03e8d9ad23a445ec
inital_vector =  FORMAT:HEX,OCT:2b3ae569c57c3eb7b95c66302267a8f5
random_string =  FORMAT:HEX,OCT:cfd0df429513022a97e76c567b3b62df6cc74fdeed1563f27b2e01646cc728f4
enc_key_salt  =  FORMAT:HEX,OCT:acca65ded29296fea498ab8a9a15aaa27445ab7c75757c99125254619e4a513b
enc_key_id    =  INTEGER:0
decryption_mode =  INTEGER:1
offfset =  INTEGER:0
blob_length =  INTEGER:361

\endcode


## Argument Options -
    - 'root_key'     : This is a mandatory parameter which is required to give
                       the key value required to sign the certificate. Key
                       file name with path required.
    - 'enckey'       : This is an optional parameter which is used only when symmetric 
                       keyring, asymmetric private or custom data is being imported via 
                       certificate. This parameter expects path to the root key and will 
                       be used in encryption of keyblob.
    - 'kd_salt'      : This parameter is only used when symmetric key, asymmetric private 
                       or custom data is being imported and salt will be used for HMAC-based 
                       key derivation. 
    - 'decrypt_mode' : This parameter is optional and specifies the AES mode used for decryption.
                       There are four supported decryption modes for keyring decryption: 
                       ECB, CBC, CFB, and CTR.             
    - 'keys_info'    : This is a mandatory parameter which is required to give
                       the json file which contains the keyring data for certificate creation.
    - rsassa_pss     : This is an optional parameter which is used if RSASSA-PSS 
                       algorithm is used for keyring certificate authentication.
    - pss_saltlen    : This is an optional parameter which is used to provide salt 
                       length if RSASSA-PSS algorithm is used for keyring certificate 
                       authentication.

## Example Use -
\code
python3 keyring_cert_gen.py  --root_key ../boot/signing/mcu_custMpk.pem --enckey ../boot/signing/mcu_custMek.key --kd_salt kd_salt.txt --keys_info keys.json
\endcode

If RSASSA-PSS algorithm for authentication of keyring certificate is required, provide the --rsassa_pss flag and --pss_saltlen **value**. Maximum supported salt length value for RSASSA-PSS is **255**. Please refer to the command below
\code
$python3 keyring_cert_gen.py --root_key ../boot/signing/mcu_custMpk.pem --keys_info keys.json --rsassa_pss --pss_saltlen 64
\endcode
