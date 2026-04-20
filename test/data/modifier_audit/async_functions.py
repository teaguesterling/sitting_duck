import asyncio

async def fetch_data(url):
    return await asyncio.sleep(1)

def sync_helper():
    return 42

async def process_items(items):
    for item in items:
        await handle(item)
