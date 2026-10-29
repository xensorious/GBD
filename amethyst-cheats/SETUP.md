# Repository and conversation handoff

The user chose the existing [xensorious/GBD](https://github.com/xensorious/GBD) repository for this project. Project files belong in `amethyst-cheats/`.

## Saving work

Use the connected GitHub tools to read the current branch, save project files, and verify the resulting commit. The cloud workspace's command-line GitHub authentication is separate from the connector; a CLI authentication failure does not establish that the connector is unavailable.

Each meaningful change should be committed to the repository with its current verification status. Local file creation alone does not save a change to GitHub. The conversation itself is not automatically copied or synchronized with the repository: these documents preserve its requirements and research as a project summary.

## What was checked during setup

On October 29, 2026, the GitHub connector could read `xensorious/GBD` and reported push permission. Its `main` branch initially contained only the root `README.md`. The cloud runtime reported `connected` and `running`, while `/workspace` was not a Git checkout. The user recalled an earlier connection error in the chat, but its exact message and cause were not established.

## Optional Codex Cloud checkout

To check out the repository in a Codex Cloud environment, use the web or desktop app under **Settings > Codex Cloud > Environments > Create environment**. Select `xensorious/GBD`, complete setup, review the results, and publish the environment. Then start a task in that environment and direct it to `amethyst-cheats/README.md`.

Existing tasks keep their own workspace state; editing a published environment does not automatically migrate this conversation or its files. Using the connected GitHub tools to save files does not require this optional checkout setup.

## References

- [OpenAI: Cloud environments](https://learn.chatgpt.com/docs/environments/cloud-environments)
