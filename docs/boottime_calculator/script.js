// Application size and secure boot time values 
let myChart = null;

//For AM261 RSA4K
const am261x_mcelf_signed_appsizes_rsa4k = [64, 128, 256, 512, 768, 1024];
const am261x_mcelf_signed_boottime_rsa4k = [43.3672, 43.8186, 44.8763, 48.5462, 52.0119, 55.6305];

const am261x_mcelf_encrypted_appsizes_rsa4k = [64, 128, 256, 512, 768, 1024];
const am261x_mcelf_encrypted_boottime_rsa4k = [44.7824, 46.5638, 50.1472, 59.0106, 67.33731, 75.94381];

//For AM261 ECDSA256
const am261x_mcelf_signed_appsizes_ecdsa256 = [64, 128, 256, 512, 768, 1024];
const am261x_mcelf_signed_boottime_ecdsa256 = [41.53810, 42.11858, 43.88324, 47.49285, 50.92885, 54.65196];

const am261x_mcelf_encrypted_appsizes_ecdsa256 = [64, 128, 256, 512, 768, 1024];
const am261x_mcelf_encrypted_boottime_ecdsa256 = [43.11100, 44.9427, 49.21772, 57.90656, 66.23975, 74.55751];

//For AM261 ECDSA384
const am261x_mcelf_signed_appsizes_ecdsa384 = [64, 128, 256, 512, 768, 1024];
const am261x_mcelf_signed_boottime_ecdsa384 = [44.76299, 44.89402, 45.80906, 49.45805, 53.12064, 56.43535];

const am261x_mcelf_encrypted_appsizes_ecdsa384 = [64, 128, 256, 512, 768, 1024];
const am261x_mcelf_encrypted_boottime_ecdsa384 = [46.08969, 47.74835, 51.23986, 59.87171, 68.23289, 76.5613];

//For AM261 ECDSA521
const am261x_mcelf_signed_appsizes_ecdsa521 = [64, 128, 256, 512, 768, 1024];
const am261x_mcelf_signed_boottime_ecdsa521 = [52.71619, 53.05267, 53.88965, 55.49442, 58.50779, 62.23696];

const am261x_mcelf_encrypted_appsizes_ecdsa521 = [64, 128, 256, 512, 768, 1024];
const am261x_mcelf_encrypted_boottime_ecdsa521 = [54.31263, 55.92145, 59.2809, 65.94943, 73.82585, 82.19795];

//For AM261 BRAINPOOL512
const am261x_mcelf_signed_appsizes_brainpool512 = [64, 128, 256, 512, 768, 1024];
const am261x_mcelf_signed_boottime_brainpool512 = [60.29023, 60.59863, 61.47898, 62.98854, 64.56535, 67.19575];

const am261x_mcelf_encrypted_appsizes_brainpool512 = [64, 128, 256, 512, 768, 1024];
const am261x_mcelf_encrypted_boottime_brainpool512 = [61.7166, 63.41366, 66.6631, 73.4446, 79.8824, 87.2032];

//For AM263x
const am263x_mcelf_signed_appsizes = [64, 128, 256, 512, 768, 1024, 1536];
const am263x_mcelf_signed_boottime = [43.0414, 46.6608, 53.9363, 68.3982, 82.9368, 97.3231, 126.6949];

const am263x_mcelf_encrypted_appsizes = [64, 128, 256, 512, 768, 1024, 1536];
const am263x_mcelf_encrypted_boottime = [44.9173, 50.1876, 60.7206, 81.6829, 102.6271, 123.5777, 165.8423];

//For AM263Px
const am263px_mcelf_signed_appsizes = [64, 128, 256, 512, 768, 1024, 1536, 2048, 2560];
const am263px_mcelf_signed_boottime = [31.0843, 31.5388, 32.5709, 34.0032, 36.4543, 39.7178, 43.8362, 48.9356, 53.1759];

const am263px_mcelf_encrypted_appsizes = [64, 128, 256, 512, 768, 1024, 1536, 2048, 2560];
const am263px_mcelf_encrypted_boottime = [32.3598, 35.1382, 38.7572, 47.4955, 56.5441, 66.4885, 83.5779, 101.8362, 119.8195];

// Information Tables
const am263x_software_table = ` Software Specifications
<table>
<tr><td>MCU+ SDK version</td><td>v10.02.00</td></tr>  
<tr><td>TIFSMCU version</td><td>v10.02.00</td></tr>  
<tr><td>Application Image Format version</td><td>MCELF</td></tr>  
<tr><td>SBL Size</td><td>44KB</td></tr>  
<tr><td>SBL Properties</td><td>Signed + Encrypted</td></tr>  
<tr><td>HSM Runtime Size</td><td>72KB</td></tr>  
<tr><td>HSM Runtime Properties</td><td>Signed + Encrypted</td></tr>  
<tr><td>Maximum Segment of Application</td><td>64KB</td></tr> 
</table>
`;

const am263px_software_table = ` Software Specifications
<table>
<tr><td>MCU+ SDK version</td><td>v10.02.00</td></tr>  
<tr><td>TIFSMCU version</td><td>v10.02.00</td></tr>  
<tr><td>Application Image Format version</td><td>MCELF</td></tr>  
<tr><td>SBL Size</td><td>60KB</td></tr>
<tr><td>SBL Properties</td><td>Signed + Encrypted</td></tr>    
<tr><td>HSM Runtime Size</td><td>62KB</td></tr>
<tr><td>HSM Runtime Properties</td><td>Signed + Encrypted</td></tr>    
<tr><td>Maximum Segment of Application</td><td>64KB</td></tr> 
</table>
`;

const am261x_software_table = ` Software Specifications
<table>
<tr><td>MCU+ SDK version</td><td>v10.02.00</td></tr>  
<tr><td>TIFSMCU version</td><td>v10.02.00</td></tr>  
<tr><td>Application Image Format version</td><td>MCELF</td></tr>  
<tr><td>SBL Size</td><td>74KB</td></tr>
<tr><td>SBL Properties</td><td>Signed + Encrypted</td></tr>    
<tr><td>HSM Runtime Size</td><td>69KB</td></tr>
<tr><td>HSM Runtime Properties</td><td>Signed + Encrypted</td></tr>    
<tr><td>Maximum Segment of Application</td><td>64KB</td></tr> 
</table>
`;

const am263x_hardware_table = ` <br>
Hardware Specifications
<table>
<tr><td>Application Core Frequency</td><td>400 MHz</td></tr>  
<tr><td>Flash Frequency</td><td>80 MHz</td></tr>
<tr><td>Flash Properties</td><td>Quad SPI Flash Interface with SDR</td></tr>
<tr><td>HSM Frequency </td><td>200 MHz</td></tr>  
</table>
`;

const am261x_hardware_table = ` <br>
Hardware Specifications
<table>
<tr><td>Application Core Frequency</td><td>500 MHz</td></tr>  
<tr><td>Flash Frequency</td><td>166 MHz</td></tr>
<tr><td>Flash Properties</td><td>Octal SPI Flash Interface with DDR support</td></tr>
<tr><td>HSM Frequency </td><td>200 MHz</td></tr>  
</table>
`;

const am263px_hardware_table = ` <br>
Hardware Specifications
<table>
<tr><td>Application Core Frequency</td><td>400 MHz</td></tr>  
<tr><td>Flash Frequency</td><td>133 MHz</td></tr>
<tr><td>Flash Properties</td><td>Octal SPI Flash Interface with DDR support</td></tr>
<tr><td>HSM Frequency </td><td>200 MHz</td></tr>  
</table>
`;

function onSubmitInfo() {
    if (myChart !== null) {
        myChart.destroy();
    }

    xValues1 = [];
    yValues1 = [];
    xValues2 = [];
    yValues2 = [];

    if (document.getElementById("mySOC").value === "am263px") {
        xValues1 = am263px_mcelf_signed_appsizes;
        yValues1 = am263px_mcelf_signed_boottime;
        xValues2 = am263px_mcelf_encrypted_appsizes;
        yValues2 = am263px_mcelf_encrypted_boottime;
        document.getElementById('table-container').innerHTML = am263px_software_table + am263px_hardware_table;
    }
    else if (document.getElementById("mySOC").value === "am263x") {
        xValues1 = am263x_mcelf_signed_appsizes;
        yValues1 = am263x_mcelf_signed_boottime;
        xValues2 = am263x_mcelf_encrypted_appsizes;
        yValues2 = am263x_mcelf_encrypted_boottime;
        document.getElementById('table-container').innerHTML = am263x_software_table + am263x_hardware_table;
    }
    else if (document.getElementById("mySOC").value === "am261x") {
        const algorithmDropdown = document.getElementById("algorithmType");
        switch (algorithmDropdown.value) {
            case "RSA4k":
                xValues1 = am261x_mcelf_signed_appsizes_rsa4k;
                yValues1 = am261x_mcelf_signed_boottime_rsa4k;
                xValues2 = am261x_mcelf_encrypted_appsizes_rsa4k;
                yValues2 = am261x_mcelf_encrypted_boottime_rsa4k;
                break;
            case "ECDSA256":
                xValues1 = am261x_mcelf_signed_appsizes_ecdsa256;
                yValues1 = am261x_mcelf_signed_boottime_ecdsa256;
                xValues2 = am261x_mcelf_encrypted_appsizes_ecdsa256;
                yValues2 = am261x_mcelf_encrypted_boottime_ecdsa256;
                break;
            case "ECDSA384":
                xValues1 = am261x_mcelf_signed_appsizes_ecdsa384;
                yValues1 = am261x_mcelf_signed_boottime_ecdsa384;
                xValues2 = am261x_mcelf_encrypted_appsizes_ecdsa384;
                yValues2 = am261x_mcelf_encrypted_boottime_ecdsa384;
                break;
            case "ECDSA521":
                xValues1 = am261x_mcelf_signed_appsizes_ecdsa521;
                yValues1 = am261x_mcelf_signed_boottime_ecdsa521;
                xValues2 = am261x_mcelf_encrypted_appsizes_ecdsa521;
                yValues2 = am261x_mcelf_encrypted_boottime_ecdsa521;
                break;
            case "BRAINPOOL512":
                xValues1 = am261x_mcelf_signed_appsizes_brainpool512;
                yValues1 = am261x_mcelf_signed_boottime_brainpool512;
                xValues2 = am261x_mcelf_encrypted_appsizes_brainpool512;
                yValues2 = am261x_mcelf_encrypted_boottime_brainpool512;
                break;
        }       
        document.getElementById('table-container').innerHTML = am261x_software_table + am261x_hardware_table;
    }

    myChart = new Chart("myChart", {
        type: "line",
        data: {
            labels: xValues1,
            datasets: [{
                label: "Authenticated Boot",
                fill: false,
                lineTension: 0,
                backgroundColor: "rgba(28, 6, 160, 0.8)",
                borderColor: "rgba(28, 6, 160, 0.8)",
                data: yValues1
            },
            {
                label: "Authenticated + Decrypted Boot",
                fill: false,
                lineTension: 0,
                backgroundColor: "rgba(18, 96, 45, 0.8)",
                borderColor: "rgba(18, 96, 45, 0.8)",
                data: yValues2
            }]
        },
        options: {
            legend: {
                display: true
            },
            scales: {
                xAxes: [{
                    scaleLabel: {
                        display: true,
                        labelString: 'Application Size in KB'
                    }
                }],
                yAxes: [{
                    scaleLabel: {
                        display: true,
                        labelString: 'Boot time in milliseconds'
                    }
                }],
            },
            animation: {
                duration: 1500,
                easing: 'easeInOutQuart'
            },
            hover: {
                mode: 'nearest',
                intersect: true
            },
            plugins: {
                title: {
                    display: true,
                    text: 'Custom Chart Title',
                    position: 'bottom',
                    padding: {
                        top: 10,
                        bottom: 30
                    }
                }
            }
        }
    });
}

function toggleAlgorithmDropdown() {
    const selectedSOC = document.getElementById("mySOC").value;
    const algorithmDropdown = document.getElementById("algorithmType");
    const algorithmLabel = document.getElementById("algorithmLabel");

    if (selectedSOC === "am261x") {
        algorithmDropdown.style.display = "inline-block";
        algorithmLabel.style.display = "inline-block";
    } else {
        algorithmDropdown.style.display = "none";
        algorithmLabel.style.display = "none";
    }
}