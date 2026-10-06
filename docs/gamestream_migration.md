# GameStream Migration
Nvidia announced that their GameStream service for Nvidia Games clients will be discontinued in February 2023.
Luckily, Sunshine performance is now equal to or better than Nvidia GameStream.

## Migration
Migrate GameStream entries by importing their working directory, command, and image into Sunshine's `apps.json` file.
Copy each box-art image into the configured covers directory
to a specified directory.

## Internet Streaming
If you are using the Moonlight Internet Hosting Tool, you can remove it from your system when you migrate to Sunshine.
To stream over the Internet with Sunshine and a UPnP-capable router, enable the UPnP option in the Sunshine Web UI.

@note{Running Sunshine together with versions of the Moonlight Internet Hosting Tool prior to v5.6 will cause UPnP
port forwarding to become unreliable. Either uninstall the tool entirely or update it to v5.6 or later.}

## Limitations
Sunshine does have some limitations, as compared to Nvidia GameStream.

* Automatic game/application list.
* Changing game settings automatically, to optimize streaming.

<div class="section_buttons">

| Previous                                        |              Next |
|:------------------------------------------------|------------------:|
| [Third-party Packages](third_party_packages.md) | [Legal](legal.md) |

</div>
