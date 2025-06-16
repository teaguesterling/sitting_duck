class Example
  def public_method
    "I'm public"
  end

  def _internal_method
    "I'm considered internal"
  end

  def check_status?
    true
  end

  def dangerous_operation!
    "With side effects!"
  end

  private

  def private_method
    "I'm private"
  end

  protected

  def protected_method
    "I'm protected"
  end
end