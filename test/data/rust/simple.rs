// Simple Rust test file for AST parsing
use std::collections::HashMap;

#[derive(Debug)]
pub struct User {
    id: u32,
    name: String,
    email: String,
}

impl User {
    pub fn new(id: u32, name: String, email: String) -> Self {
        User { id, name, email }
    }
    
    pub fn get_name(&self) -> &str {
        &self.name
    }
    
    fn validate_email(&self) -> bool {
        self.email.contains('@')
    }
}

pub enum Status {
    Active,
    Inactive,
    Pending,
}

pub trait Validate {
    fn is_valid(&self) -> bool;
}

impl Validate for User {
    fn is_valid(&self) -> bool {
        !self.name.is_empty() && self.validate_email()
    }
}

pub fn create_user(name: &str, email: &str) -> Result<User, String> {
    if name.is_empty() {
        return Err("Name cannot be empty".to_string());
    }
    
    let user = User::new(1, name.to_string(), email.to_string());
    
    if user.is_valid() {
        Ok(user)
    } else {
        Err("Invalid user data".to_string())
    }
}

pub mod utils {
    pub fn format_name(name: &str) -> String {
        name.trim().to_lowercase()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_user_creation() {
        let user = create_user("John Doe", "john@example.com");
        assert!(user.is_ok());
    }
}