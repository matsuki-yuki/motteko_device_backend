import os
import google.generativeai as genai
from firebase_functions import storage_fn
from firebase_admin import initialize_app, storage

# 👇 先ほど作った config.py を読み込む
import config

# Firebaseの初期化
initialize_app()

# config.py から値を呼び出して設定する
genai.configure(api_key=config.GEMINI_API_KEY)

@storage_fn.on_object_finalized(bucket=config.MY_BUCKET)
def check_forgotten_items(event: storage_fn.CloudEvent[storage_fn.StorageObjectData]) -> None:
    bucket_name = event.data.bucket
    file_name = event.data.name

    print(f"✅ 新しい画像を検知: {file_name}")

    try:
        bucket = storage.bucket(bucket_name)
        blob = bucket.blob(file_name)
        image_bytes = blob.download_as_bytes()

        model = genai.GenerativeModel('gemini-2.5-flash')
        
        prompt = """
        これは玄関のカメラで撮影された、外出時の持ち物の写真です。
        写っているアイテムをチェックしてください。
        特に「財布」「鍵（キーケース）」「スマートフォン」が写っているか確認し、
        結果を簡潔に教えてください。
        """

        image_part = {
            "mime_type": "image/jpeg",
            "data": image_bytes
        }
        
        print("🤖 Geminiに画像解析をリクエスト中...")
        
        response = model.generate_content([prompt, image_part])
        
        print("🌟 AIの判定結果:")
        print(response.text)

    except Exception as e:
        print(f"❌ エラーが発生しました: {e}")