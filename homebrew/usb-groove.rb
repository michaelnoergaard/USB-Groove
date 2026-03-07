# Homebrew Cask formula for USB Groove
# To use: brew tap michaelnoergaard/usb-groove && brew install usb-groove
#
# To set up the tap, create a repo named "homebrew-usb-groove" containing:
#   Casks/usb-groove.rb  (copy of this file with the correct sha256)

cask "usb-groove" do
  version :latest
  sha256 :no_check

  url "https://github.com/michaelnoergaard/USB-Groove/releases/latest/download/USBGroove-macOS.zip"
  name "USB Groove"
  desc "Menu bar app that automatically plays MP3 files from USB drives"
  homepage "https://github.com/michaelnoergaard/USB-Groove"

  depends_on macos: ">= :monterey"

  app "USBGroove.app"

  zap trash: [
    "~/Library/Logs/USBGroove.log",
  ]
end
