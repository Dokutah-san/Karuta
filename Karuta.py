import asyncio
import discord
import requests
from deep_translator import GoogleTranslator

client = discord.Client()
ESP_IP = "http://192.168.100.126"

# Sistem pengunci ID krm sesuai kode aslimu
last_krm_message_id = None

def send_to_esp(endpoint, params=None):
    """Helper untuk mengirim data ke ESP8266 tanpa blocking"""
    try:
        response = requests.get(f"{ESP_IP}/{endpoint}", params=params, timeout=3)
        if response.status_code == 200:
            print(f"[ESP] Berhasil memperbarui /{endpoint}")
    except Exception as e:
        print(f"[ESP ERROR] Gagal kontak /{endpoint}:", e)

def amblas_data_time(text):
    """Mengekstrak sisa waktu dari teks pengingat Karuta ke dalam variabel menit"""
    menit_daily = 0
    menit_vote = 0
    menit_drop = 0
    menit_grab = 0
    
    lines = text.lower().split('\n')
    for line in lines:
        clean = line.replace("`", "").replace("*", "")
        
        # 1. Ambil data Daily
        if "daily" in clean:
            if "ready" not in clean:
                angka = [int(s) for s in clean.split() if s.isdigit()]
                if angka:
                    menit_daily = angka[0] * 60 if "hour" in clean else angka[0]

        # 2. Ambil data Vote
        elif "vote" in clean:
            if "ready" not in clean:
                angka = [int(s) for s in clean.split() if s.isdigit()]
                if angka:
                    menit_vote = angka[0] * 60 if "hour" in clean else angka[0]

        # 3. Ambil data Drop
        elif "drop" in clean:
            if "ready" not in clean:
                angka = [int(s) for s in clean.split() if s.isdigit()]
                if angka:
                    menit_drop = angka[0] * 60 if "hour" in clean else angka[0]

        # 4. Ambil data Grab
        elif "grab" in clean:
            if "ready" not in clean:
                angka = [int(s) for s in clean.split() if s.isdigit()]
                if angka:
                    menit_grab = angka[0] * 60 if "hour" in clean else angka[0]

    payload = {
        "daily": menit_daily,
        "vote": menit_vote,
        "drop": menit_drop,
        "grab": menit_grab
    }
    return payload

@client.event
async def on_ready():
    print(f"Selfbot login sebagai: {client.user}")

@client.event
async def on_message(message):
    global last_krm_message_id
    pesan_teks = message.content.lower()

    # ------------------ FITUR 1: TRANSLATE via REPLY ------------------
    if message.author.id == client.user.id:
        if message.content.startswith("_translate"):
            if message.reference and message.reference.message_id:
                try:
                    replied_message = await message.channel.fetch_message(message.reference.message_id)
                    teks_asal = replied_message.content
                    if teks_asal:
                        hasil_translate = GoogleTranslator(source='auto', target='id').translate(teks_asal)
                        await message.edit(content=f"📝 **Terjemahan:** {hasil_translate}")
                        print(f"[TRANSLATE] Sukses menerjemahkan pesan.")
                except Exception as e:
                    print(f"[TRANSLATE ERROR]: {e}")
            return # Keluar agar tidak mengganggu pendeteksian krm di bawah

    # ------------------ FITUR 2: COOLDOWN MANUAL KETUKAN USER ------------------
    if client.user.mentioned_in(message):
        if "your answer" in pesan_teks:
            await asyncio.get_event_loop().run_in_executor(None, send_to_esp, "daily_cooldown")
        elif "voting " in pesan_teks:
            await asyncio.get_event_loop().run_in_executor(None, send_to_esp, "vote_cooldown")
        elif "is dropping" in pesan_teks:
            await asyncio.get_event_loop().run_in_executor(None, send_to_esp, "drop_cooldown")
        elif "took the" in pesan_teks and "fought off" not in pesan_teks:
            await asyncio.get_event_loop().run_in_executor(None, send_to_esp, "grab_cooldown")

    # Catat ID saat kita mengetik krm secara mandiri
    if message.author.id == client.user.id:
        if message.content.lower().strip() == "krm":
            last_krm_message_id = message.id
            print(f"[KRM] Command milik kita terkirim: {message.id}")

    # ------------------ FITUR 3: SINKRONISASI COOLDOWN BALASAN KARUTA ------------------
    if message.author.id == 646937666251915264: # Jika pesan dari Karuta
        
        # PENGUNCI AMAN: Karuta harus me-reply pesan krm milik kita sendiri
        if message.reference and message.reference.message_id == last_krm_message_id:
            print("[KRM] Reply ke command krm milik kita terdeteksi!")

            teks_raksasa = message.content + "\n"
            if message.embeds:
                for embed in message.embeds:
                    if embed.title: teks_raksasa += f"\n{embed.title}"
                    if embed.description: teks_raksasa += f"\n{embed.description}"
                    if embed.author and embed.author.name: teks_raksasa += f"\n{embed.author.name}"
                    if embed.footer and embed.footer.text: teks_raksasa += f"\n{embed.footer.text}"
                    if embed.fields:
                        for f in embed.fields:
                            teks_raksasa += f"\n{f.name} {f.value}"

            data_waktu = amblas_data_time(teks_raksasa)
            print(f"[SINKRONISASI] {data_waktu}")

            await asyncio.get_event_loop().run_in_executor(
                None, send_to_esp, "sync_krm", data_waktu
            )

# Token Discord
TOKEN = "MTUxMDk2NjExMDU3MTQ2MjY2Ng.GL-mv4._k43R5TcMh-ju97-F-FIMCq7pCQCXsfARfK5tc"
client.run(TOKEN)