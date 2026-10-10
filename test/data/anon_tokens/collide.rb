module Demo
  def run(xs)
    if xs.empty?
      return nil
    elsif xs.size == 1
      yield xs.first
    else
      xs.each do |x|
        next if x.nil?
        break unless x
      end
    end

    case xs.size
    when 0 then :none
    else :many
    end

    while xs.any?
      xs.pop
    end

    until xs.empty?
      xs.shift
    end

    for i in 0..2
      redo if false
    end

    begin
      raise "x"
    rescue StandardError => e
      retry if false
    ensure
      nil
    end
  end
end
