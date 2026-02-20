# GoToBP

**GoToBP** is a plugin for Unreal Engine that allows you to create clickable hyperlinks to exact locations inside a Blueprint graph.

With GoToBP, you can generate a URL pointing to a specific node position in a Blueprint. Share that link with teammates, and when they open it, Unreal Engine will automatically navigate them to the same location in the Blueprint graph.

Perfect for code reviews, bug reports, documentation, and team collaboration.

---

## ✨ Features

- 🔗 Generate a hyperlink to an exact location in a Blueprint graph  
- 📋 Automatically copies the link to your system clipboard  
- 🖱️ Simple activation workflow using a customizable hotkey  
- 👥 Share links with teammates for instant navigation  
- ⚙️ Easy setup through Editor Settings  

---

## 📦 Installation

1. Download or clone this repository.
2. Place the `GoToBP` plugin folder into your project's: \<ProjectRoot\>/Plugins/
3. Open your project in Unreal Engine.
4. If prompted, enable the plugin.
5. Restart the editor if required.

That’s it — no additional setup required.

---

## ⚙️ Configuration

After installing the plugin:

1. Open **Editor Settings**.
2. Locate the **GoToBP** section.
3. Configure your **Activation Key**.

The Activation Key determines when link creation mode is triggered.

---

## 🚀 How It Works

1. Tap your configured **Activation Key** (do not hold it).
2. Left-click on any location inside a Blueprint graph.
3. GoToBP will:
- Generate a hyperlink to that exact location.
- Automatically copy the link to your system clipboard.

You can now paste and share the link anywhere (Slack, email, documentation, issue trackers, etc.).

When someone clicks the link, Unreal Engine navigate them directly to the referenced Blueprint graph location.

---

## 🧠 Example Use Cases

- Code reviews  
- Bug reports  
- Technical documentation  
- Team discussions  
- Onboarding new developers  
- Sharing exact node references  
