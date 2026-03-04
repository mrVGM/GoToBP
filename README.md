# GoToBP

**GoToBP** is a plugin for Unreal Engine that allows you to create clickable hyperlinks to exact locations inside a Blueprint graph.

With GoToBP, you can generate a URL pointing to a specific node in a Blueprint. Share that link with teammates, and when they open it, Unreal Engine will automatically navigate them to the same location in the Blueprint graph.

Perfect for code reviews, bug reports, documentation, and team collaboration.

---

## ✨ Features

- 🔗 Generate a hyperlink to an exact location in a Blueprint graph  
- 📋 Automatically copies the link to your system clipboard  
- 🖱️ Quick access directly from the Blueprint node context menu  
- 👥 Share links with teammates for instant navigation  

---

## 📦 Installation

1. Download or clone this repository.
2. Place the `GoToBP` plugin folder into your project's: `<ProjectRoot>/Plugins/`
3. Open your project in Unreal Engine.
4. If prompted, enable the plugin.
5. Restart the editor if required.

That’s it — no additional setup required.

---

## 🚀 How It Works

1. Open any Blueprint graph.
2. Right-click on a node.
3. In the context menu, click **"Get Location Link"**.
4. GoToBP will:
   - Generate a hyperlink to that exact node location.
   - Automatically copy the link to your system clipboard.

You can now paste and share the link anywhere (Slack, email, documentation, issue trackers, etc.).

When someone clicks the link, Unreal Engine will navigate them directly to the referenced Blueprint graph location.

---

## 🧠 Example Use Cases

- Code reviews  
- Bug reports  
- Technical documentation  
- Team discussions  
- Onboarding new developers  
- Sharing exact node references  
