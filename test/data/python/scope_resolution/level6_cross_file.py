# Level 6: Cross-file import resolution
# References should resolve through import to level6_models.py

from level6_models import User, create_user, DEFAULT_ADMIN
import level6_models

# Direct import usage:
# User → level6_models.User class
# create_user → level6_models.create_user function
# DEFAULT_ADMIN → level6_models.DEFAULT_ADMIN variable

def make_admin():
    # User() should resolve to level6_models.User
    admin = User("Admin", DEFAULT_ADMIN)
    return admin

def make_user(name, email):
    # create_user should resolve to level6_models.create_user
    user = create_user(name, email)
    # user.display_name() requires:
    # 1. create_user returns User (from level6_models)
    # 2. display_name is User.display_name
    print(user.display_name())
    return user

# Module-qualified access:
# level6_models.User should resolve to User class in that module
def make_via_module():
    user = level6_models.User("Test", "test@example.com")
    return user
