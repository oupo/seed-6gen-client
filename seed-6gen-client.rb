require "json"
require "net/http"
require "open3"

def main
  pass = (File.read("password").chomp rescue "")

  list = JSON.parse(Net::HTTP.get(URI.parse("https://seed-6gen.herokuapp.com/list")))
  puts "There are #{list.size} entries."

  return if list.empty?

  found = []

  open("input", "w") do |f|
    f.puts list.size
    list.each do |entry|
      f.puts "#{entry["id"]} #{entry["iv0"].join(" ")} #{entry["iv1"].join(" ")}"
    end
  end

  system("./seed-6gen-search", 0 => "input")

  result = {}
  open("output", "r") do |f|
    if /\A(\d+) (\d+) (\d+) ([0-9a-f]+)\z/ =~ f.gets.chomp
      result[$1.to_i] = [$2.to_i, $3.to_i, $4.to_i(16)]
    else
      raise "output file parse error"
    end
  end

  list.each do |entry|
    id = entry["id"]
    seed = nil
    if result[id]
      seed = result[id][2]
    end
    puts "id#{id}:"
    data = {"id" => id.to_s, "seed" => seed ? "%08x" % seed : "notfound", "pass" => pass}
    puts Net::HTTP.post_form(URI.parse("https://seed-6gen.herokuapp.com/answer"), data).body
  end
end


main() if $0 == __FILE__