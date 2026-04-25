async fn fetch_data(url: &str) -> String {
    reqwest::get(url).await.unwrap().text().await.unwrap()
}

fn sync_helper() -> i32 {
    42
}

pub async fn public_async(item: &str) -> bool {
    true
}

unsafe fn unsafe_helper() {
    std::ptr::null::<i32>();
}

pub fn public_sync() -> &'static str {
    "hello"
}
